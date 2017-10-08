#include "renderer.h"
#include <3ds.h>
#include <cstdio>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include "vshader_shbin.h"

//this code is from https://smealum.github.io/ctrulib/graphics_2printing_2system-font_2source_2main_8c-example.html
//but with a few changes

Renderer::Renderer() {
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

	//create the render target
	target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetClear(target, C3D_CLEAR_ALL, 0x2C2F33FF, 0);
	C3D_RenderTargetSetOutput(target, GFX_TOP, GFX_LEFT,
		(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
			GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
			GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))
		);

	//load and bind shader
	shaderBin = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_len);
	shaderProgramInit(&program);
	shaderProgramSetVsh(&program, &shaderBin->DVLE[0]);
	C3D_BindProgram(&program);

	//get uniform positions
	uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");

	// Configure attributes for use with the vertex shader
	C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
	AttrInfo_Init(attrInfo);
	AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
	AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord

	// Compute the projection matrix
	Mtx_OrthoTilt(&projection, 0.0, 400.0, 240.0, 0.0, 0.0, 1.0, true);

	// Configure depth test to overwrite pixels with the same depth (needed to draw overlapping glyphs)
	C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);

	const Result fontMappedValue = fontEnsureMapped();
	if (R_FAILED(fontMappedValue))
		printf("fontEnsureMapped failed: %08lX\n", fontMappedValue);

	// Load the glyph texture sheets
	TGLP_s* glyphInfo = fontGetGlyphInfo();
	//glyphSheets = (C3D_Tex*)malloc(sizeof(C3D_Tex)*glyphInfo->nSheets);
	glyphSheets = std::vector<C3D_Tex>(glyphInfo->nSheets);//for some malloc causes issues here
	for (int i = 0; i < glyphInfo->nSheets; i++) {
		C3D_Tex* tex = &glyphSheets[i];
		tex->data = fontGetGlyphSheetTex(i);
		tex->fmt = (GPU_TEXCOLOR)glyphInfo->sheetFmt;
		tex->size = glyphInfo->sheetSize;
		tex->width = glyphInfo->sheetWidth;
		tex->height = glyphInfo->sheetHeight;
		tex->param = GPU_TEXTURE_MAG_FILTER(GPU_LINEAR) | GPU_TEXTURE_MIN_FILTER(GPU_LINEAR)
			| GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE) | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE);
	}

	// Create the text vertex array
	textVtxArray = (textVertex_s*)linearAlloc(sizeof(textVertex_s)*TEXT_VTX_ARRAY_COUNT);
}

Renderer::~Renderer() {
	//free(glyphSheets);
	shaderProgramFree(&program);
	DVLB_Free(shaderBin);
	C3D_Fini();
}

void Renderer::setTextColor(u32 color) {
	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(env, C3D_RGB, GPU_CONSTANT, 0, 0);
	C3D_TexEnvSrc(env, C3D_Alpha, GPU_TEXTURE0, GPU_CONSTANT, 0);
	C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
	C3D_TexEnvFunc(env, C3D_RGB, GPU_REPLACE);
	C3D_TexEnvFunc(env, C3D_Alpha, GPU_MODULATE);
	C3D_TexEnvColor(env, color);
}

void Renderer::addTextVertex(float vx, float vy, float tx, float ty) {
	textVertex_s* vtx = &textVtxArray[textVtxArrayPos++];
	vtx->position[0] = vx;
	vtx->position[1] = vy;
	vtx->position[2] = 0.5f;
	vtx->texcoord[0] = tx;
	vtx->texcoord[1] = ty;
}

void Renderer::wrapText(float x, float right, float scaleX, std::string & text) {
	unsigned int lastSpace = 0;
	float firstX = x;
	for (ssize_t i = 0; text[i] != '\0';) {
		uint32_t code;
		const uint8_t c = text[i];
		const ssize_t units = decode_utf8(&code, &c);
		if (units == -1)
			return;
		if (code == ' ') {//store the last space
			lastSpace = i;
		} else if (code == '\n') {
			x = firstX; //reset x and last Space
			lastSpace = 0;
			i += 1;
			continue;
		}
		int glyphIdx = fontGlyphIndexFromCodePoint(code);
		charWidthInfo_s* info = fontGetCharWidthInfo(glyphIdx);
		x += info->charWidth * scaleX;//get the width of the char
		if (right < x) {//check if the char is off screen
			if (lastSpace == 0) {
				text.insert(i, "\n");
				i += 1;
			} else {//turn last space into end line
				text[lastSpace] = '\n';
				i = lastSpace + 1;
			}
			x = firstX; //reset x and last Space
			lastSpace = 0;
			continue;
		}
		i += units;
	}
}

void Renderer::renderText(float x, float y, float scaleX, float scaleY, bool baseline, const char* text) {
	// Configure buffers
	C3D_BufInfo* bufInfo = C3D_GetBufInfo();
	BufInfo_Init(bufInfo);
	BufInfo_Add(bufInfo, textVtxArray, sizeof(textVertex_s), 2, 0x10);

	const uint8_t* p = (const uint8_t*)text;
	float firstX = x;
	u32 flags = GLYPH_POS_CALC_VTXCOORD | (baseline ? GLYPH_POS_AT_BASELINE : 0);
	int lastSheet = -1;
	uint32_t code;
	do {
		if (!*p) break;
		const ssize_t units = decode_utf8(&code, p);
		if (units == -1)
			break;
		p += units;
		if (code == '\n') {
			x = firstX;
			y += scaleY*fontGetInfo()->lineFeed;
		} else if (0 < code) {
			int glyphIdx = fontGlyphIndexFromCodePoint(code);
			fontGlyphPos_s data;
			fontCalcGlyphPos(&data, glyphIdx, flags, scaleX, scaleY);
			
			// Bind the correct texture sheet
			if (data.sheetIndex != lastSheet) {
				lastSheet = data.sheetIndex;
				C3D_TexBind(0, &glyphSheets[lastSheet]);
			}
			int arrayIndex = textVtxArrayPos;
			if (TEXT_VTX_ARRAY_COUNT <= (arrayIndex + 4))
				break; // We can't render more characters
			
			// Add the vertices to the array
			addTextVertex(x + data.vtxcoord.left,  y + data.vtxcoord.bottom, data.texcoord.left,  data.texcoord.bottom);
			addTextVertex(x + data.vtxcoord.right, y + data.vtxcoord.bottom, data.texcoord.right, data.texcoord.bottom);
			addTextVertex(x + data.vtxcoord.left,  y + data.vtxcoord.top,    data.texcoord.left,  data.texcoord.top);
			addTextVertex(x + data.vtxcoord.right, y + data.vtxcoord.top,    data.texcoord.right, data.texcoord.top);
			// Draw the glyph
			C3D_DrawArrays(GPU_TRIANGLE_STRIP, arrayIndex, 4);
			x += data.xAdvance;
		}
	} while (0 < code);
}

void Renderer::addMessageToQueue(std::string message) {
	wrapText(0, TOP_SCREEN_WIDTH, 0.5, message);
	textToRender.push_front({ 0.0f, 0.0f, 0.5, 0.5, message });
	isThereNewTextToDraw = true;
}

void Renderer::render() {
	if (!isThereNewTextToDraw) return; //only draw when there's new text to draw
	isThereNewTextToDraw = false;

	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	textVtxArrayPos = 0; // Clear the text vertex array
	C3D_FrameDrawOn(target);

	// Update the uniforms
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);

	//render text
	setTextColor(0xFFC5C4C3);
	float textTopLeftPosition = TOP_SCREEN_HEIGHT - (0.10 * fontGetInfo()->lineFeed);

	textToRender.remove_if([&](Text text) {
		if (textTopLeftPosition < 0.0f) return true; //remove text that's off screen
		textTopLeftPosition -= (std::count(text.content.begin(), text.content.end(), '\n') + 1) * (text.scaleY * fontGetInfo()->lineFeed);
		renderText(text.x, textTopLeftPosition, text.scaleX, text.scaleY, false, text.content.c_str());
		return false;
	});

	C3D_FrameEnd(0);
}
