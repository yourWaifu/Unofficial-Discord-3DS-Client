#include "3ds_discord.h"
#include <algorithm>

ThreeDSDiscordClient::ThreeDSDiscordClient(const std::string token) :
	ThreeDSWebsocketClient(token),
	currentServerHandle(0),
	currentChannelHandle(0)
{

}

void ThreeDSDiscordClient::addNewMessage(SleepyDiscord::Message message) {
	renderer.addMessageToQueue(message.author.username + ":\n" + message.content);
}

void ThreeDSDiscordClient::onMessage(SleepyDiscord::Message message) {
	if (servers.size() == 0 || isCurrentChannelHandleValid == false) return;
	if (message.channelID == getCurrentChannel().ID) {
		addNewMessage(message);
	}
}

void ThreeDSDiscordClient::onServer(SleepyDiscord::Server server) {
	addServer(server);
}

void ThreeDSDiscordClient::switchServer(int displacement) {
	if (servers.size() == 0) return;
	//change current server
	currentServerHandle += displacement;
	if (-1 == currentServerHandle)
		currentServerHandle = servers.size() - 1;
	else if (servers.size() <= currentServerHandle)
		currentServerHandle = 0;
	isCurrentChannelHandleValid = false;
	printf("Switched Server to %s.\n", getCurrentServer().name.c_str());
}

void ThreeDSDiscordClient::switchChannel(int displacement) {
	if (servers.size() == 0) return;
	unsigned int newChannelPosition;
	/*if the current channel handle is invalid
	  then make it valid*/
	if (isCurrentChannelHandleValid == false) {
		if (getCurrentServer().channels.size() == 0)
			getCurrentServer().channels = GetServerChannels(getCurrentServer().ID);
		//change channel to first channel in that server
		newChannelPosition = 0;
		isCurrentChannelHandleValid = true;
	} else {
		newChannelPosition = getCurrentChannel().position + displacement;
	}

	//get the position of the next channel
	
	const unsigned int channelCount = getCurrentServer().channels.size();
	if (channelCount <= newChannelPosition)
		newChannelPosition = 0;
	do {
		//get the channel in that position
		for (currentChannelHandle = 0; currentChannelHandle < channelCount; ++currentChannelHandle) {
			if (getCurrentChannel().position == newChannelPosition
				&&
				getCurrentChannel().type == SleepyDiscord::Channel::SERVER_TEXT
				) {
				printf("Switched channel to %s #%s\n", getCurrentServer().name.c_str(), getCurrentChannel().name.c_str());
				return;
			}
		}
		newChannelPosition = 0;
	} while (true);
}

void ThreeDSDiscordClient::loadMessages() {
	if (servers.size() == 0 || isCurrentChannelHandleValid == false) return;
	std::vector<SleepyDiscord::Message> messages = getMessages(getCurrentChannel().ID, limit, "", 8);
	for (std::vector<SleepyDiscord::Message>::reverse_iterator i = messages.rbegin(), end = messages.rend(); i != end; ++i) {
		addNewMessage(*i);
	}
}

void ThreeDSDiscordClient::launchKeyboardAndSentMessage() {
	if (servers.size() == 0 || isCurrentChannelHandleValid == false) return;
	//tell discord that we are typing
	if (sendTyping(getCurrentChannel().ID) == false) //error check
		return;
	//init keyboard
	const int numOfButtons = 2;
	const int messageCharLimit = 2000;
	SwkbdState keyboard;
	swkbdInit(&keyboard, SWKBD_TYPE_NORMAL, numOfButtons, messageCharLimit);
	//set the hint to the name of the channel
	std::string keyboardHint;
	const char* keyboardHintStart = "Message #";
	keyboardHint.reserve(sizeof(keyboardHintStart) + getCurrentChannel().name.length());
	keyboardHint += keyboardHintStart;
	keyboardHint += getCurrentChannel().name;
	swkbdSetHintText(&keyboard, keyboardHint.c_str());
	//back to init keyboard
	swkbdSetInitialText(&keyboard, "");
	swkbdSetFeatures(&keyboard, SWKBD_DEFAULT_QWERTY | SWKBD_DARKEN_TOP_SCREEN);
	swkbdSetValidation(&keyboard, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
	//get input from the keyboard
	std::vector<char> buffer(messageCharLimit);
	SwkbdButton button = swkbdInputText(&keyboard, &buffer[0], messageCharLimit);
	//read input
	if (button != SWKBD_BUTTON_CONFIRM)
		return;	//return if user did not press confirm
	auto iteratorToZero = std::find(buffer.begin(), buffer.end(), 0); //find end of string
	if (iteratorToZero == buffer.end()) {
		printf("message doesn't end");
		return;
	}
	//send message
	sendMessage(getCurrentChannel().ID, std::string(buffer.begin(), iteratorToZero));
}

void ThreeDSDiscordClient::onReady(std::string * jsonMessage) {
	printf("ready\n");
	if (isBot() == false && servers.size() == 0) {
		std::vector<SleepyDiscord::Server> servers = getServers();
		//you should some error checking here
		for (SleepyDiscord::Server server : servers) {
			addServer(server);
		}
	}
}
