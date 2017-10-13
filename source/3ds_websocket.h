#pragma once
#include <3ds.h>
#include <wslay/wslay.h>
#include <sleepy_discord/sleepy_discord.h>
#include "renderer.h"
#include <vector>
#include <string>

class ThreeDSWebsocketClient : public SleepyDiscord::BaseDiscordClient {
public:
	ThreeDSWebsocketClient() {}
	ThreeDSWebsocketClient(const std::string token);
	~ThreeDSWebsocketClient();
	bool pollSocket(int events);
	void tick();
	sslcContext sslc_context;
	wslay_event_context_ptr eventContext;
	wslay_event_callbacks callbacks;
	inline bool shouldContinue() {
		return isRunning;
	}
	inline void close() {
		isRunning = false;
	}

private:

	//call back
	static void onMessageCallback(wslay_event_context_ptr ctx,
		const struct wslay_event_on_msg_recv_arg *arg,
		void *user_data);

	void onError(SleepyDiscord::ErrorCode errorCode, const std::string errorMessage);
	void sleep(const unsigned int milliseconds);
	bool connect(const std::string & uri);
	void disconnect(unsigned int code, const std::string reason);
	void send(std::string message);
	int sock;
	bool isRunning = false;

	std::vector<SleepyDiscord::Server> serverList;

	std::string selectedServerID = "";
	std::string selectedChannelID = "";
};