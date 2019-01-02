
#include "window.h"

#include "shaderprintf.h"

const int screenw = 1600, screenh = 900;

int main() {

	OpenGL context(screenw, screenh, "bench", false);

	LARGE_INTEGER start, current, frequency;
	QueryPerformanceCounter(&start);
	QueryPerformanceFrequency(&frequency);

	bool loop = true;

	Buffer printBuffer = createPrintBuffer();

	while (loop) {

		MSG msg;

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
		


		swapBuffers();
	}
	return 0;
}
