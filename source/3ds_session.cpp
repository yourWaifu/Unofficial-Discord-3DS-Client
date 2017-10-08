#include "3ds_session.h"
#include <string>
#include <cstring>
#include "netdb.h"
#include "poll.h"

//init the custom session
SleepyDiscord::CustomInit SleepyDiscord::Session::init = 
[]()->SleepyDiscord::GenericSession* { return new ThreeDSSession; };

//for errors 0x10 is for closing sockets, and 0x04 is for destroying ssl
enum SockError {
	CLOSE_SOCKET = 0x10,
	DESTROY_SSL  = 0x4,
	CLOSE_SSL_SOCKET = CLOSE_SOCKET | DESTROY_SSL
};

SleepyDiscord::Response ThreeDSSession::request(const SleepyDiscord::RequestMethod method) {
	int sock = -1;
	sslcContext context;
	SleepyDiscord::Response target = request(method, sock, context, false);
	sslcDestroyContext(&context);
	closesocket(sock);
	return target;
}

SleepyDiscord::Response ThreeDSSession::request(const SleepyDiscord::RequestMethod method, int& sock, sslcContext& context, bool cleanUp, bool readBody) {
	//cleaning up in the case something doesn't work
	const SleepyDiscord::Response target = requestInteral(method, sock, context, readBody);
	if (cleanUp == true && target.statusCode < 100) {
		if (target.statusCode & DESTROY_SSL)
			sslcDestroyContext(&context);
		if (target.statusCode & CLOSE_SOCKET)
			closesocket(sock);
	}
	return target;
}

SleepyDiscord::Response ThreeDSSession::requestInteral(const SleepyDiscord::RequestMethod _method, int& sock, sslcContext& sslContext, bool readBody) {
	//a useful const variable
	const std::string newLine = "\r\n";
	//setup targets
	SleepyDiscord::Response target;
	//make response buffer
	std::string response;
	unsigned int responseBufferSize = 0x2000;	//our header size limit
	std::vector<char> responseBuffer(responseBufferSize);
	//connect to server
	//get hostname and protocol from uri, thanks uwebsockets for the help
	do {
		std::string hostname, path;
		std::string protocol = url.substr(0, url.find("://"));
		size_t offset = protocol.length() + 3;
		if (offset < url.length()) {	//makes sure that there is a hostname
			hostname = url.substr(offset, url.find("/", offset) - offset);

			offset += hostname.length();
			if (url[offset] == '/')
				path = url.substr(offset);
		}
		//get addresses
		sock = -1;
		addrinfo hints;
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_IP;
		addrinfo *res;
		const int getaddrinfo_returned = getaddrinfo(hostname.c_str(), "443", &hints, &res);
		if (getaddrinfo_returned != 0) 	//error check
			return { SleepyDiscord::AN_ERROR, "{\"code\":0,\"message\":\"getaddrinfo: " +
				std::string(gai_strerror(getaddrinfo_returned)) + "\"}",{} };
		//it's possible to get addresses that don't work, so we need to loop through the list
		//til one of them works
		for (addrinfo *current = res; current; current = current->ai_next) {
			sock = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
			if (sock == -1)
				continue;
			if (::connect(sock, current->ai_addr, current->ai_addrlen) == 0)
				break;	//success
			closesocket(sock);
			sock = -1;
		}
		//free up memory from addrinfo
		freeaddrinfo(res);
		if (sock == -1) {	//error check
			return { SleepyDiscord::AN_ERROR, "{\"code\":431,\"message\":\"Could not connect to the host\"}",{} };
		}

		//setup ssl
		Result returnValue = sslcCreateContext(&sslContext, sock, SSLCOPT_DisableVerify, hostname.c_str());
		if (returnValue < 0)
			return { CLOSE_SOCKET, "{\"code\":2,\"message\":\"sslcCreateContext failed " + std::to_string(returnValue) + "\"}",{} };
		int retval = -1;
		u32 sslConnectionOut;
		returnValue = sslcStartConnection(&sslContext, &retval, &sslConnectionOut);
		if (returnValue < 0)
			return { CLOSE_SSL_SOCKET, "{\"code\":0,\"message\":\"sslcStartConnection failed " + std::to_string(returnValue) + "\"}",{} };

		//setup some method varables
		const char* methodNames[] = { "POST", "PATCH", "DELETE", "GET", "PUT" };
		const std::string method = methodNames[_method];
		const std::string httpVersion = readBody ? " HTTP/1.0\r\n" : " HTTP/1.1\r\n";	//reading bodies from 1.1 isn't fully supported
		const std::string hostHeaderField = "Host: " + hostname + newLine;
		const std::string acceptHeaderField = "Accept: */*\r\n";
		std::string connectionHeaderField = "Connection: close\r\n";
		if (head != nullptr)
			for (SleepyDiscord::HeaderPair pair : *head)	//check if there's a connection header
				if (strcmp(pair.name, "Connection") == 0) {
					connectionHeaderField = "";
					break;
				}

		//get size of the request, it should be faster then reallocating memory all the time
		unsigned int requestSize = method.size();
		requestSize += 1 + path.size();	//the 1+ is for the space between method and path
		requestSize += 1 + httpVersion.size();

		requestSize += hostHeaderField.size();
		requestSize += acceptHeaderField.size();
		requestSize += connectionHeaderField.size();
		if (head != nullptr)
			for (SleepyDiscord::HeaderPair pair : *head)	//get size of header
				requestSize += strlen(pair.name) + pair.value.size() + 4;	//the 4 chars are : /r/n
		requestSize += 2;	//end of header
		if (body != nullptr)
			requestSize += body->size();	//get body size

		//make the request
		std::string rawRequest;
		rawRequest.reserve(requestSize);

		rawRequest += method;
		rawRequest += ' ';
		rawRequest += path;
		rawRequest += httpVersion;

		rawRequest += hostHeaderField;
		rawRequest += acceptHeaderField;
		rawRequest += connectionHeaderField;
		if (head != nullptr)
			for (SleepyDiscord::HeaderPair pair : *head) {
				rawRequest += pair.name;
				rawRequest += ": ";
				rawRequest += pair.value;
				rawRequest += newLine;
			}
		rawRequest += newLine;	//end of header


		if (body != nullptr)
			rawRequest += *body;

		//send the request
		returnValue = sslcWrite(&sslContext, rawRequest.c_str(), rawRequest.length());
		if (returnValue < 0)
			return { CLOSE_SSL_SOCKET, "{\"code\":0,\"message\":\"sslcWrite failed " + std::to_string(returnValue) + "\"}", {} };

		//get the header
		returnValue = sslcRead(&sslContext, &responseBuffer[0], responseBufferSize, !readBody);
		if (returnValue < 0)
			return { CLOSE_SSL_SOCKET, "{\"code\":431,\"message\":\"sslcRead failed " + std::to_string(returnValue) + "\"}",{} };
		response = std::string(responseBuffer.begin(), responseBuffer.begin() + returnValue);
		//find the end of the header
		const unsigned int endOfHeader = response.find("\r\n\r\n");
		if (endOfHeader == std::string::npos)
			return { 431, "{\"code\":431,\"message\":\"Couldn't find end of header, because header is too large\"}", {} };

		//get the status code
		const unsigned int statusCodeStart = response.find(' ') + 1;
		target.statusCode = std::stoi(response.substr(statusCodeStart, 3));	//3 ditgits is how large a http status code is

		//read status code and loop back if asked to redirect
		switch (target.statusCode) {
		case 301: case 302: case 303: case 307: case 308: {//codes for redirection
			const std::string locationLeft = "Location: ";
			const size_t locationPos = response.find(locationLeft);
			//get new location
			url = response.substr(locationPos + locationLeft.length(), response.find(newLine, locationPos) - locationPos);
		}
		continue;	//loop back
		break;
		default: break;
		}
		break;
	} while (true);

	//fill in response header
	size_t offset = response.find(newLine) + newLine.length();
	for (size_t newLinePos = response.find(newLine, offset);
		newLinePos != offset;	//condition
		newLinePos = response.find(newLine, offset)
		) {
		const std::string headerFieldLeft = response.substr(offset, response.find(':', offset) - offset);
		offset += headerFieldLeft.length();
		while (response[++offset] == ' ');	//ignore whitespace
		const std::string headerFieldRight = response.substr(offset, newLinePos - offset);
		target.header[headerFieldLeft] = headerFieldRight;
		offset = newLinePos + newLine.length();
	}
	offset += newLine.length();

	//skip reading body if requested to do so
	if (readBody == false) {
		const int returnValue = sslcRead(&sslContext, &responseBuffer[0], offset, false);
		if (returnValue < 0)	//error check
			return { CLOSE_SSL_SOCKET,
			"{\"code\":0,\"message\":\"sslcRead failed while reading the header "
			"and readBody was set to false "
			+ std::to_string(returnValue) + "\"}",{} };
		//I dont know what to put into text, so lets just put the response in there
		target.text = response;
		return target;
	}

	//get the body
	/* The start of the body is after the \r\n\r\n, so we need to get the begining
	   of body from the first sslcRead which happened before we got the header.
	   We'll then add the the remaining reponse and the rest of the body in a buffer
	   together to get the body.
	*/
	if (target.header.count("Content-Length")) {
		/* For responses that have content length, we can easly get the rest of the
		   responses because we know when where the body ends.
		 */
		const size_t bodyLength = std::stoi(target.header["Content-Length"]);
		target.text.reserve(bodyLength);
		target.text += response.substr(offset, bodyLength);
		if (target.text.length() < bodyLength) {	//check that we have the whole body
			const unsigned int bodyBytesLeft = bodyLength - target.text.length();
			std::vector<char> responseBuffer(bodyBytesLeft);
			const int returned = sslcRead(&sslContext, &responseBuffer[0], bodyBytesLeft, false);
			if (returned < 0)	//error check
				return { CLOSE_SSL_SOCKET,
				"{\"code\":0,\"message\":\"sslcRead failed while getting the body "
				"while Content-Length was available "
				+ std::to_string(returned) + "\"}",{} };
			target.text += std::string(responseBuffer.begin(), responseBuffer.end());
		}
	} else {
		/* In the event there is no Content-Length, we would have to assume that the
		   server will signal the end when the connection is closed
		*/
		std::vector<u8> buffer(0x1000);
		unsigned int size = 0;
		int returned;
		while (0 < (returned = sslcRead(&sslContext, &buffer[size], 0x1000, false))) {
			size += returned;
			buffer.resize(size + 0x1000);
		}
		if (0 != returned && returned != -660555771)  //error check
			return { CLOSE_SSL_SOCKET,
			"{\"code\":0,\"message\":\"sslcRead failed while getting the body "
			"without Content-Length "
			+ std::to_string(returned) + "\"}",{} };
		target.text.reserve((response.length() - offset) + size);
		target.text += response.substr(offset);
		target.text += std::string(buffer.begin(), buffer.end());
	}

	//done
	return target;
}
