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
	void onReady(std::string* jsonMessage);
	void onHeartbeat() {
		printf("heartbeat sent\n");
	}
	void onHeartbeatAck() {
		printf("heartbeat acknowledged\n");
	}
	void onMessage(SleepyDiscord::Message message);
	void onServer(SleepyDiscord::Server server);

	inline void addServer(SleepyDiscord::Server server) {
		servers.push_back(server);
		printf("Added %s to the server list\n", server.name.c_str());
	}

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
	bool isCurrentChannelHandleValid = false;

	void addNewMessage(SleepyDiscord::Message message);
};