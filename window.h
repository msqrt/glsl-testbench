
#pragma once

#include <Windows.h>
#include <string>
#include <fstream>

#include <d3d12.h>

void setupD3D();
void closeD3D();
void setupWindow(int width, int height, const std::string& title, bool fullscreen);
void closeWindow();
void swapBuffers();
void setTitle(const std::string& title);

POINT getMouse();
void setMouse(POINT);

struct D3D {
	D3D() { setupD3D(); }
	~D3D() { closeD3D(); }
};

struct Window {
	D3D d3d;
	Window(int width, int height, const std::string& title, bool fullscreen) { setupWindow(width, height, title, fullscreen); }
	~Window() { closeWindow(); }
};
