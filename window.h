
#pragma once

#include <Windows.h>
#include <string>
#include <fstream>

#include <gl/gl.h>
#include "loadgl/glext.h"
#include "loadgl/wglext.h"
#include "loadgl/loadgl46.h"


void setupGL(int width, int height, const std::string& title, bool fullscreen);
void closeGL();
void swapBuffers();
void setTitle(const std::string& title);

POINT getMouse();
void setMouse(POINT);

struct OpenGL {
	OpenGL(int width, int height, const std::string& title, bool fullscreen) { setupGL(width, height, title, fullscreen); }
	~OpenGL() { closeGL(); }
};
