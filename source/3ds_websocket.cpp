#include <fcntl.h>
#include "3ds_websocket.h"
#include "3ds_session.h"
#include "netdb.h"
#include "poll.h"

/*
* This struct is passed as *user_data* in callback function.  The
* *fd* member is the file descriptor of the connection to the client.
*/
ssize_t recv_callback(wslay_event_context_ptr ctx, uint8_t *buf, size_t len,
                      int flags, void *user_data) {
	ThreeDSWebsocketClient *client = (ThreeDSWebsocketClient*)user_data;
	ssize_t returnValue = sslcRead(&client->sslc_context, buf, len, flags == 2);

	if (returnValue <= 0) {
		if (returnValue == -666847230 || errno == EAGAIN) {//would block
			errno = EWOULDBLOCK;
			wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
			return -1;
		} else
			wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);

		if (!returnValue) {
			printf("sslcWrite EOF");
			client->close();
		}

		printf("sslcRead fail: %08X\n", returnValue);
		return -1;
	} else return returnValue;
}

ssize_t send_callback(wslay_event_context_ptr ctx,
                      const uint8_t *data, size_t len, int flags,
                      void *user_data) {
	ThreeDSWebsocketClient *client = (ThreeDSWebsocketClient*)user_data;

	ssize_t returnValue = sslcWrite(&client->sslc_context, data, len);
	printf("write%i\n", len);
	if (returnValue <= 0) {
		if (returnValue == -666847230 || errno == EAGAIN) {//would block
			errno = EWOULDBLOCK;
			wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
		} else wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);

		if (!returnValue) {
			printf("sslcWrite EOF");
			client->close();
		}

		printf("sslcWrite fail: %08X\n", returnValue);
		return -1;
	} else return returnValue;
}

int mask_callback(wslay_event_context_ptr ctx, unsigned char* buf, unsigned int len, void* user_data) {
	sslcGenerateRandomData(buf, len);
	return 0;
}

void ThreeDSWebsocketClient::onMessageCallback(wslay_event_context_ptr ctx,
                       const struct wslay_event_on_msg_recv_arg *arg,
                       void *user_data) {
	ThreeDSWebsocketClient *client = (ThreeDSWebsocketClient*)user_data;
	client->processMessage(std::string(reinterpret_cast<const char*>(arg->msg)));
}

ThreeDSWebsocketClient::ThreeDSWebsocketClient(const std::string token) {
	callbacks = {
		recv_callback,
		send_callback,
		mask_callback,
		NULL,
		NULL,
		NULL,
		onMessageCallback
	};
	eventContext = nullptr;
	start(token, 1);
}

ThreeDSWebsocketClient::~ThreeDSWebsocketClient() {
	disconnect(1000, "Deliberate disconnection");
}

bool ThreeDSWebsocketClient::pollSocket(int events) {
	pollfd pol;
	pol.fd = sock;
	pol.events = events;

	if (poll(&pol, 1, 0) == 1)
		return pol.revents & events;
	return false;
}

void ThreeDSWebsocketClient::tick() {
	heartbeat();
	//when a heartbeat fails, eventContext will be null because we're disconnected
	if (isRunning == false)
		return;

	if (pollSocket(POLLIN) && wslay_event_want_read(eventContext)) {
		if (wslay_event_recv(eventContext))
			return;	//error maybe I don't know
	}

	if (pollSocket(POLLOUT) && wslay_event_want_write(eventContext)) {
		if (wslay_event_send(eventContext))
			return; //error maybe
	}
}

void ThreeDSWebsocketClient::onError(SleepyDiscord::ErrorCode errorCode, const std::string errorMessage) {
	printf("Error %i: %s\n", errorCode, errorMessage.c_str());
}

void ThreeDSWebsocketClient::sleep(const unsigned int milliseconds) {
	//I really don't know how to make the 3ds sleep
	for (float i = 0; i < (milliseconds / 60) + 1 && aptMainLoop(); i++) {
		gspWaitForVBlank();
	}
}

bool sslErrorCheck(const char* functionName, const Result returnValue, int sockfd, sslcContext *context = nullptr) {
	if (returnValue < 0) {
		switch (context == nullptr) { //fall through
		case false: sslcDestroyContext(context);
		case true : closesocket(sockfd);
		}
		printf("%s returned: %08X\n", functionName, returnValue);
		return SleepyDiscord::AN_ERROR;
	}
	return SleepyDiscord::NO_ERRORS;
}

bool ThreeDSWebsocketClient::connect(const std::string & uri) {
	//start up websockets
	if (eventContext == nullptr)
		wslay_event_context_client_init(&eventContext, &callbacks, this);
	isRunning = true;

	ThreeDSSession session;
	session.setUrl(uri);

	//Handshaking TIME!!!!
	//u8 randomString[29];
	char randomKey[25] = "x3JJHMbDL1EzLkh9GBhXDw==";	//trust me it's random
	//sslcGenerateRandomData(randomString, 25);

	std::vector<SleepyDiscord::HeaderPair> header = {
		{ "Upgrade", "websocket" },
		{ "Connection", "Upgrade" },
		{ "Sec-WebSocket-Key", randomKey },
		{ "Sec-WebSocket-Version", "13" }
	};
	session.setHeader(header);

	//request to connect to websocket
	SleepyDiscord::Response response = session.request(SleepyDiscord::Get, sock, sslc_context, true, false);

	if (response.statusCode != SleepyDiscord::SWITCHING_PROTOCOLS) { //error check
		printf("Websocket connection Error: %s\n", response.text.c_str());
		return SleepyDiscord::AN_ERROR;
	}

	//I don't know what this does, but it doesn't work without it
	int status = fcntl(sock, F_GETFL);
	fcntl(sock, F_SETFL, status | O_NONBLOCK);

	return SleepyDiscord::NO_ERRORS;
}

void ThreeDSWebsocketClient::disconnect(unsigned int code, const std::string reason) {
	//close wslay
	if (eventContext != nullptr) {
		const void* reasonPointer = reason.c_str();
		wslay_event_queue_close(eventContext, code, static_cast<const uint8_t*>(reasonPointer), reason.length());
		wslay_event_send(eventContext);
		wslay_event_context_free(eventContext);
		eventContext = nullptr;
	}

	isRunning = false;

	//close everything else
	if (sock) {
		sslcDestroyContext(&sslc_context);
		closesocket(sock);
		sock = -1;
	}

	return;
}

void ThreeDSWebsocketClient::send(std::string message) {
	const void* messagePointer = message.c_str();
	struct wslay_event_msg msg = { 1, static_cast<const uint8_t*>(messagePointer), message.length() };
	const int returned = wslay_event_queue_msg(eventContext, &msg);
	if (returned != 0) { //error
		printf("Send error: ");
		switch (returned) {
		case WSLAY_ERR_NO_MORE_MSG     : printf("Could not queue given message\n"); break;
		case WSLAY_ERR_INVALID_ARGUMENT: printf("The given message is invalid\n" ); break;
		case WSLAY_ERR_NOMEM           : printf("Out of memory\n"                ); break;
		default                        : printf("unknown\n"                      ); break;
		}
	}
}
