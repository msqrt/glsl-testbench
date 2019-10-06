
#pragma once

#include <Windows.h>
#include <string>
#include <fstream>

#include <gl/gl.h>
#include "loadgl/glext.h"
#include "loadgl/wglext.h"
#include "loadgl/loadgl46.h"

void setupGL(int width, int height, const std::string& title, bool fullscreen, bool show);
void closeGL();
bool loop();
bool glOpen();
void swapBuffers();
void setTitle(const std::string& title);
void showWindow();
void hideWindow();

bool keyDown(UINT vk_code);
bool keyHit(UINT vk_code);
void resetHits();

POINT getMouse();
void setMouse(POINT);

// todo: make this a true class? or use some kind of "finally" for the close?
// possible reasoning: don't really want singleton, but don't really want to support multiple contexts either
// why not just global functions: to be nice, we want to free all RAII objects before closing the GL context, so this has to be RAII as well
struct OpenGL {
	OpenGL(
		int width, int height,
		const std::string& title = "",
		bool fullscreen = false,
		bool show = true)
	{
		if (glOpen()) return;
		isOwning = true;
		setupGL(width, height, title, fullscreen, show);
	}
	~OpenGL() {
		if(isOwning)
			closeGL();
	}
	bool isOwning = false;
};
