#pragma once
#include <citro3d.h>
#include <string>
#include <list>
#include <vector>

#define TOP_SCREEN_WIDTH 400
#define TOP_SCREEN_HEIGHT 240

#define BOTTOM_SCREEN_WIDTH 320
#define BOTTOM_SCREEN_HEIGHT 240


class Renderer {
public:
	Renderer();
	~Renderer();

	void setTextColor(u32 color);
	void wrapText(float x, float right, float scaleX, std::string & text);
	void renderText(float x, float y, float scaleX, float scaleY, bool baseline, const char* text);
	void addMessageToQueue(std::string message);
	void render();
private:
	void addTextVertex(float vx, float vy, float tx, float ty);

	struct textVertex_s { 
		float position[3];
		float texcoord[2];
	};

	shaderProgram_s program;
	s8 uLoc_projection;

	C3D_RenderTarget* target;
	DVLB_s* shaderBin;
	textVertex_s* textVtxArray;
	int textVtxArrayPos;
	C3D_Mtx projection;
	std::vector<C3D_Tex> glyphSheets;

	const int TEXT_VTX_ARRAY_COUNT = (4 * 1024);

	struct Text {
		float x;
		float y;
		float scaleX;
		float scaleY;
		std::string content;
	};
	std::list<Text> textToRender;
	bool isThereNewTextToDraw = false;
};