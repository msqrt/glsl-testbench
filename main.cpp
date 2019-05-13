
#include "window.h"
#include "shaderprintf.h"
#include "gl_helpers.h"
#include "math_helpers.h"
#include "gl_timing.h"
#include "math.hpp"
#include "text_renderer.h"
#include <dwrite_3.h>
#include <d2d1_3.h>
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "winmm.lib")

#include <vector>
#include <map>
#include <array>

const int screenw = 1600, screenh = 900;

#include <random>

extern LPARAM down[];
extern int downptr;

int main() {

	OpenGL context(screenw, screenh, "", false);

	LARGE_INTEGER start, current, frequency;
	QueryPerformanceFrequency(&frequency);

	Font font(L"Consolas");

	bool loop = true;
	unsigned frame = 0;

	QueryPerformanceCounter(&start);
	while (loop) {

		MSG msg;

		for (int i = 0; i < downptr; ++i)
			down[i] |= 1 << 30;

		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			switch (msg.message) {
			case WM_QUIT:
				loop = false;
				break;
			}
		}

		QueryPerformanceCounter(&current);
		float t = float(double(current.QuadPart - start.QuadPart) / double(frequency.QuadPart));
		
		TimeStamp start;

		float off = 30.f;

		for (int i = 0; i < downptr; ++i) {
			WCHAR name[256];
			GetKeyNameTextW(down[i], name, 256);
			std::wstring n(name);
			font.drawText(n + ((((down[i]>>30)&1)==0) ? L" hit!": L" down!"), 10.f, off, 15.f);
			off += 25.f;
		}

		TimeStamp end;

		font.drawText(L"⏱: " + std::to_wstring(elapsedTime(start, end)) + L", " + std::to_wstring(downptr) + L" keys down", 5.f, 5.f, 15.f);


		off = 5.f;

		for (int i = 0; i < joyGetNumDevs(); ++i) {
			JOYCAPSW caps;
			joyGetDevCapsW(i, &caps, sizeof(caps));

			if (std::wstring(caps.szPname).length() == 0) continue;

			JOYINFOEX kek;
			kek.dwSize = sizeof(kek);
			kek.dwFlags = JOY_RETURNALL;
			joyGetPosEx(i, &kek);
			std::wstring mask;
			for (int i = caps.wNumButtons-1; i >= 0; --i)
				mask = mask + std::to_wstring((kek.dwButtons>>i)&1);

			float x = float(kek.dwXpos - caps.wXmin) / float(caps.wXmax - caps.wXmin);
			float y = float(kek.dwYpos - caps.wYmin) / float(caps.wYmax - caps.wYmin);
			float z = float(kek.dwZpos - caps.wZmin) / float(caps.wZmax - caps.wZmin);
			float u = float(kek.dwUpos - caps.wUmin) / float(caps.wUmax - caps.wUmin);
			float v = float(kek.dwVpos - caps.wVmin) / float(caps.wVmax - caps.wVmin);
			float r = float(kek.dwRpos - caps.wRmin) / float(caps.wRmax - caps.wRmin);

			font.drawText(caps.szPname + std::wstring(L", buttons: ") + mask +
				L", x: " + std::to_wstring(x) +
				L", y: " + std::to_wstring(y) +
				L", z: " + std::to_wstring(z) +
				L", u: " + std::to_wstring(u) +
				L", v: " + std::to_wstring(v) +
				L", r: " + std::to_wstring(r), 205.f, off, 15.f);
			off += 25.f;
		}


		swapBuffers();
		glClearColor(.1f, .1f, .1f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	return 0;
}
