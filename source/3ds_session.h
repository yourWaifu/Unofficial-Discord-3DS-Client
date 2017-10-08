#pragma once
#include <3ds.h>
#include "sleepy_discord/session.h"
#include "sleepy_discord/error.h"
#include "3ds/services/sslc.h"

class ThreeDSSession : public SleepyDiscord::GenericSession {
public:
	ThreeDSSession() {}
	inline void setUrl(const std::string& _url) { url = _url; printf("url is set to: %s\n", url.c_str()); }
	inline void setBody(const std::string* jsonParamters) { 
		body = jsonParamters;
	}
	inline void setHeader(const std::vector<SleepyDiscord::HeaderPair>& header) {
		head = &header;
	}
	inline void setMultipart(const std::initializer_list<SleepyDiscord::Part>& parts) {
	}
	inline SleepyDiscord::Response Post  () { return request(SleepyDiscord::Post); }
	inline SleepyDiscord::Response Patch () { return { SleepyDiscord::BAD_REQUEST, "", {} }; }
	inline SleepyDiscord::Response Delete() { return { SleepyDiscord::BAD_REQUEST, "", {} }; }
	inline SleepyDiscord::Response Get   () { return request(SleepyDiscord::Get); }
	inline SleepyDiscord::Response Put   () { return { SleepyDiscord::BAD_REQUEST, "", {} }; }
	SleepyDiscord::Response request(const SleepyDiscord::RequestMethod method);
	SleepyDiscord::Response request(const SleepyDiscord::RequestMethod method, int& sock, sslcContext& sslContext, bool cleanUp = true, bool readBody = true);
private:
	SleepyDiscord::Response requestInteral(const SleepyDiscord::RequestMethod method, int& sock, sslcContext& sslContext, bool readBody);

	std::string url;
	const std::vector<SleepyDiscord::HeaderPair>* head = nullptr;
	const std::string* body = nullptr;
};