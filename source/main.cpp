#include <3ds.h>
#include <malloc.h>
#include <memory>
#include "gol.h"
#include "token_file.h"
#include "3ds_discord.h"

//time related
#define TICKS_PER_SEC 268123480.0
#define TICKS_PER_MILLISEC 268123.4800

void exit() {
	sslcExit();
	socExit();
	httpcExit();
	acExit();
	consoleClear();
	gfxExit();
}

int main() {
	atexit(exit);

	//start up some stuff
	gfxInitDefault();
	gfxSet3D(false);
	consoleInit(GFX_BOTTOM, NULL);

#ifdef DEBUGGING
	printf("starting up discord\n");
	for (int i = 0; i < 180 && aptMainLoop(); i++) {
		gspWaitForVBlank();
	}
#endif

	//check that we are connected to wifi
	acInit();
	u32 wifiStatus;
	ACU_GetWifiStatus(&wifiStatus);
	if (!wifiStatus) {
		printf("WARNING: No wifi detected\nNow waiting for wifi\n");
		gfxFlushBuffers();
		do {
			gspWaitForVBlank();
			ACU_GetWifiStatus(&wifiStatus);
		} while (!wifiStatus && aptMainLoop());
		if (!aptMainLoop()) return EXIT_SUCCESS; //the user asked to close
	}

	//start http
	httpcInit(0x400000);
	//start ssl
	sslcInit(0);

	//start sockets
	const size_t align = 0x1000;
	const size_t socketBufferSize = 0x100000;
	std::unique_ptr<u32> socketsBuffer((u32*)memalign(align, socketBufferSize));
	if (!R_SUCCEEDED(socInit(socketsBuffer.get(), socketBufferSize))) {
		printf("error could not start sockets");
		//I don't know if this is the right way to wait a sec, but oh well
		for (int i = 0; i < 60 && aptMainLoop(); i++) {
			gspWaitForVBlank();
		}
		return EXIT_FAILURE;
	}

	//start the discord client
	tokenFile token("discord token.txt");
	if (token.getSize() < 0) { //error check
		printf("Error: could not find file called\n"
			"discord token.txt in the root of your\n"
			"SD card. Please place a bot token in\n"
			"that file.");
		gfxFlushBuffers();
		while (aptMainLoop()) {
			hidScanInput();
			u32 keysPressedDown = hidKeysDown();
			if (keysPressedDown)
				return EXIT_SUCCESS;
			gspWaitForVBlank();
		}
	}
	ThreeDSDiscordClient client(token.getToken());
	//clientPointer = &client;
	token.close();

	//main loop
	printf("press start to exit\n");
	gfxSwapBuffers();
	while (aptMainLoop() && client.shouldContinue()) {
		hidScanInput();
		u32 keysPressedDown = hidKeysDown();
		if (keysPressedDown & KEY_START)
			break;
		if (keysPressedDown & KEY_DLEFT)//to do make a better ui
			client.switchServer(-1);
		if (keysPressedDown & KEY_DRIGHT)
			client.switchServer(1);
		if (keysPressedDown & KEY_DDOWN)
			client.switchChannel();
		if (keysPressedDown & KEY_Y)
			client.loadMessages();
		if (keysPressedDown & KEY_A)
			client.launchKeyboardAndSentMessage();
		client.tick();
		client.renderer.render();
		gspWaitForVBlank();
	}

	return EXIT_SUCCESS;
}