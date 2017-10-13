#pragma once
#include "3ds_websocket.h"

class ThreeDSDiscordClient : public ThreeDSWebsocketClient {
public:
	ThreeDSDiscordClient() {}
	ThreeDSDiscordClient(const std::string token);
	void switchServer();
	void switchChannel();
	void loadMessages();
	void launchKeyboardAndSentMessage();
	Renderer renderer;
private:
	void onReady(std::string* jsonMessage) {
		printf("ready\n");
	}
	void onHeartbeat() {
		printf("heartbeat sent\n");
	}
	void onHeartbeatAck() {
		printf("heartbeat acknowledged\n");
	}
	void onMessage(SleepyDiscord::Message message);
	void onServer(SleepyDiscord::Server server);

	inline SleepyDiscord::Server& getCurrentServer() {
		return servers[currentServerHandle];
	}

	inline SleepyDiscord::Channel& getCurrentChannel() {
		return getCurrentServer().channels[currentChannelHandle];
	}

	//discord stuff
	std::vector<SleepyDiscord::Server> servers;
	//SleepyDiscord::Channel currentChannel;
	unsigned int currentServerHandle;
	unsigned int currentChannelHandle;

	void addNewMessage(SleepyDiscord::Message message);
};