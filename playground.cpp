
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

#include <vector>
#include <map>
#include <array>

const int screenw = 1600, screenh = 900;

#include <iostream>

#include <io.h>
#include <fcntl.h>
#include <sstream>
#include <locale>
#include <codecvt>

extern WPARAM down[];
extern std::wstring text;
extern int downptr, hitptr;

bool keyDown(UINT vk_code);
bool keyHit(UINT vk_code);

// smoke
int main_smoke() {

	OpenGL context(screenw, screenh, "fluid", false);

	LARGE_INTEGER start, current, frequency;
	QueryPerformanceFrequency(&frequency);

	Font font(L"Consolas");

	bool loop = true;
	unsigned frame = 0;

	Program simulate = createProgram("shaders/gridSimulate.glsl");
	Program draw = createProgram("shaders/gridBlitVert.glsl", "", "", "", "shaders/gridBlitFrag.glsl");

	QueryPerformanceCounter(&start);
	float prevTime = .0f;

	int res = 512;
	Texture<GL_TEXTURE_2D> velocity[3], value[2], divergence, pressure[2], density;
	Texture<GL_TEXTURE_2D> fluidVolume, boundaryVelocity;
	for (int i = 0; i < 3; ++i)
		glTextureStorage2D(velocity[i], 1, GL_RG32F, res, res);
	for (int i = 0; i < 2; ++i) {
		glTextureStorage2D(value[i], 1, GL_RGBA16F, res, res);
		glTextureStorage2D(pressure[i], 1, GL_R32F, res, res);
	}

	glTextureStorage2D(divergence, 1, GL_R32F, res, res);
	glTextureStorage2D(density, 1, GL_RGBA32F, res, res); // density required: 
	glTextureStorage2D(fluidVolume, 1, GL_RGBA32F, res, res);
	glTextureStorage2D(boundaryVelocity, 1, GL_RG32F, res, res);

	//Texture<GL_TEXTURE_2D> meme = loadImage(L"assets/välkky.png");

	vec4 camPosition(.0f, .0f, 0.f, 1.f);
	vec2 viewAngle(.0f, .0f);
	bool mouseDrag = false;
	ivec2 mouseOrig;
	ivec2 mouse;
	mat4 previousCam;
	mat4 proj = projection(screenw / float(screenh), 80.f, .2f, 15.0f);

	int ping = 0;
	glUseProgram(simulate);
	glUniform1i("mode", 0);
	//bindTexture("meme", meme);
	bindImage("result", 0, value[ping], GL_WRITE_ONLY, GL_RGBA16F);
	bindImage("velocity", 0, velocity[2], GL_WRITE_ONLY, GL_RG32F);
	glDispatchCompute(res / 16, res / 16, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	Buffer printBuffer;
	glNamedBufferStorage(printBuffer, sizeof(int) * 1024 * 1024, nullptr, GL_DYNAMIC_STORAGE_BIT);

	float phase = 1.f;

	const float dt = .01f;
	float t = .0f;

	while (loop) {

		MSG msg;

		hitptr = 0;

		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			switch (msg.message) {
			case WM_QUIT:
				loop = false;
				break;
			case WM_LBUTTONDOWN:
				mouseDrag = true;
				mouseOrig = ivec2(msg.lParam & 0xFFFF, msg.lParam >> 16);
				//ShowCursor(false);
				break;
			case WM_MOUSEMOVE:
				if (mouseDrag) {
					viewAngle += vec2(ivec2(msg.lParam & 0xFFFF, msg.lParam >> 16) - mouseOrig)*.005f;
					setMouse(POINT{ mouseOrig.x, mouseOrig.y });
				}
				break;
			case WM_LBUTTONUP:
				//ShowCursor(true);
				mouseDrag = false;
				break;
			}
		}

		while (viewAngle.x > pi) viewAngle.x -= 2.f*pi;
		while (viewAngle.x < -pi) viewAngle.x += 2.f*pi;
		viewAngle.y = clamp(viewAngle.y, -.5*pi, .5*pi);


		mat4 cam = yRotate(viewAngle.x) * xRotate(-viewAngle.y);
		vec3 cameraVelocity(float(keyDown('D') - keyDown('A')), float(keyDown('E') - keyDown('Q')), float(keyDown('S') - keyDown('W')));
		cam.col(3) = camPosition += cam * vec4(cameraVelocity*.02f, .0f);
		auto icam = invert(cam);
		auto iproj = invert(proj);

		QueryPerformanceCounter(&current);
		//float t = float(double(current.QuadPart - start.QuadPart) / double(frequency.QuadPart));

		float off = 5.f;

		TimeStamp start;

		glUseProgram(simulate);
		glUniform1i("mode", 1);
		glUniform1f("t", t);
		glUniform1f("dt", dt);
		t += dt;
		bindImage("result", 0, value[1 - ping], GL_WRITE_ONLY, GL_RGBA16F);
		bindImage("velocity", 0, velocity[1 - ping], GL_WRITE_ONLY, GL_RG32F);
		bindTexture("oldVelocity", velocity[2]);
		bindTexture("oldResult", value[ping]);
		//bindPrintBuffer(simulate, printBuffer);
		glDispatchCompute(res / 16, res / 16, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		//printf("%s\n", getPrintBufferString(printBuffer).c_str());

		ping = 1 - ping;

		glUniform1i("mode", 2);
		bindImage("velocity", 0, velocity[1 - ping], GL_WRITE_ONLY, GL_RG32F);
		bindImage("fluidVolume", 0, fluidVolume, GL_WRITE_ONLY, GL_RGBA32F);
		bindImage("boundaryVelocity", 0, boundaryVelocity, GL_WRITE_ONLY, GL_RG32F);
		bindTexture("oldVelocity", velocity[ping]);
		bindImage("result", 0, value[1 - ping], GL_WRITE_ONLY, GL_RGBA16F);
		bindTexture("oldResult", value[ping]);
		glDispatchCompute(res / 16, res / 16, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		ping = 1 - ping;
		int pressurePing = 0;

		glUniform1i("mode", 3);
		bindImage("result", 0, value[1 - ping], GL_WRITE_ONLY, GL_RGBA16F);
		bindTexture("oldResult", value[ping]);
		bindImage("pressure", 0, pressure[0], GL_WRITE_ONLY, GL_R32F);
		bindImage("divergenceImage", 0, divergence, GL_WRITE_ONLY, GL_R32F);
		bindImage("fluidVolume", 0, fluidVolume, GL_READ_ONLY, GL_RGBA32F);
		bindImage("boundaryVelocity", 0, boundaryVelocity, GL_READ_ONLY, GL_RG32F);
		bindTexture("oldVelocity", velocity[ping]);
		glDispatchCompute(res / 16, res / 16, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);


		for (int i = 0; i < 128; ++i) {
			glUniform1i("mode", 4);
			bindImage("pressure", 0, pressure[1 - pressurePing], GL_WRITE_ONLY, GL_R32F);
			bindTexture("oldPressure", pressure[pressurePing]);
			bindTexture("divergence", divergence);
			bindTexture("fluidVol", fluidVolume);
			glDispatchCompute(res / 16, res / 16, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			pressurePing = 1 - pressurePing;
		}

		glUniform1i("mode", 5);
		bindTexture("oldPressure", pressure[pressurePing]);
		bindImage("velocity", 0, velocity[1 - ping], GL_WRITE_ONLY, GL_RG32F);
		bindImage("velDifference", 0, velocity[2], GL_WRITE_ONLY, GL_RG32F);
		bindTexture("oldVelocity", velocity[ping]);
		bindImage("boundaryVelocity", 0, boundaryVelocity, GL_READ_ONLY, GL_RG32F);
		bindTexture("fluidVol", fluidVolume);
		glUniform1f("phase", phase);
		//phase = 1.f-phase;
		glDispatchCompute(res / 16, res / 16, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		ping = 1 - ping;

		TimeStamp end;

		glUseProgram(draw);
		bindTexture("oldPressure", pressure[pressurePing]);
		bindTexture("divergence", divergence);
		bindTexture("result", value[ping]);
		bindTexture("velocity", velocity[ping]);
		bindTexture("fluidVolume", fluidVolume);
		bindTexture("boundaryVelocity", boundaryVelocity);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		font.drawText(std::to_wstring(prevTime) + L"ms", 5.f, 5.f, 15.f, screenw - 5);

		/*std::vector<uint> getter(N * 8);
		glGetNamedBufferSubData(hashCounts, 0, sizeof(uint)*N * 8, getter.data());
		int cumsum = 0; int maximum = 0, maxi = 0;
		for (int i = 0; i < getter.size(); ++i)
			if (getter[i] != 0) {
				if (getter[i] > maximum) {
					maximum = getter[i];
					maxi = i;
				}
				//printf("%d:%d\t(%d)\n", i, getter[i], cumsum += getter[i]);
			}
		printf("max %d at %d\n", maximum, maxi);*/
		swapBuffers();
		prevTime = elapsedTime(start, end);

		glClearColor(.0f, .0f, .0f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	return 0;
}



int main_sort() {

	OpenGL context(screenw, screenh, "", false);

	LARGE_INTEGER start, current, frequency;
	QueryPerformanceFrequency(&frequency);

	Program sort = createProgram("shaders/sort.glsl");

	const int N = 1024 * 1024;

	Buffer buffer[2];
	glNamedBufferStorage(buffer[0], sizeof(int) * 2 * N, nullptr, 0);
	glNamedBufferStorage(buffer[1], sizeof(int) * 2 * N, nullptr, 0);

	glUseProgram(sort);
	glUniform1i("mode", -1);
	bindBuffer("inputBuffer", buffer[0]);
	bindBuffer("outputBuffer", buffer[1]);
	glDispatchCompute(128, 1, 1);

	int ping = 0;
	//for (int i = 0; i < 10; ++i) {

	int pong = 1 - ping;

	glUniform1i("mode", 0);
	bindBuffer("inputBuffer", buffer[ping]);
	bindBuffer("outputBuffer", buffer[pong]);
	glDispatchCompute(128, 1, 1);

	std::vector<int> input(2 * N), result(2 * N);

	glGetNamedBufferSubData(buffer[ping], 0, sizeof(int)*input.size(), input.data());
	glGetNamedBufferSubData(buffer[pong], 0, sizeof(int)*result.size(), result.data());

	for (int i = 0; i < 32; ++i)
		printf("(%d, %d) - (%d, %d)\n", input[i * 2], input[i * 2 + 1], result[i * 2], result[i * 2 + 1]);

	ping = pong;
	//}

	system("pause");
	return 0;
}


int main_particles() {

	OpenGL context(screenw, screenh, "", false);

	LARGE_INTEGER start, current, frequency;
	QueryPerformanceFrequency(&frequency);

	Font font(L"Consolas");

	bool loop = true;
	unsigned frame = 0;

	Program simulate = createProgram("shaders/simulate.glsl"), bookkeeper = createProgram("shaders/bookkeep.glsl");
	Program drawParticles = createProgram("shaders/particleVert.glsl", "", "", "", "shaders/particleFrag.glsl");

	QueryPerformanceCounter(&start);
	float prevTime = .0f;

	const int N = 1024 * 1024;
	Buffer posBuffer[2], velBuffer[2];
	glNamedBufferStorage(posBuffer[0], sizeof(float) * 4 * N, nullptr, 0);
	glNamedBufferStorage(velBuffer[0], sizeof(float) * 4 * N, nullptr, 0);
	glNamedBufferStorage(posBuffer[1], sizeof(float) * 4 * N, nullptr, 0);
	glNamedBufferStorage(velBuffer[1], sizeof(float) * 4 * N, nullptr, 0);

	Buffer hashIndices, hashCounts, hashOffsets;
	glNamedBufferStorage(hashIndices, sizeof(int) * (8 * N + 1), nullptr, 0);
	glNamedBufferStorage(hashCounts, sizeof(int) * 8 * N, nullptr, 0);
	glNamedBufferStorage(hashOffsets, sizeof(int) * 8 * N, nullptr, 0);

	vec4 camPosition(.0f, .0f, 0.f, 1.f);
	vec2 viewAngle(.0f, .0f);
	bool mouseDrag = false;
	ivec2 mouseOrig;
	ivec2 mouse;
	mat4 previousCam;
	mat4 proj = projection(screenw / float(screenh), 80.f, .2f, 15.0f);

	glUseProgram(simulate);
	glUniform1i("mode", 0);
	bindBuffer("posBuffer", posBuffer[0]);
	bindBuffer("velBuffer", velBuffer[0]);
	int ping = 0;
	bindBuffer("hashIndices", hashIndices);
	bindBuffer("hashCounts", hashCounts);
	bindBuffer("hashOffsets", hashOffsets);
	glDispatchCompute(128, 1, 1);

	while (loop) {

		MSG msg;

		hitptr = 0;

		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			switch (msg.message) {
			case WM_QUIT:
				loop = false;
				break;
			case WM_LBUTTONDOWN:
				mouseDrag = true;
				mouseOrig = ivec2(msg.lParam & 0xFFFF, msg.lParam >> 16);
				ShowCursor(false);
				break;
			case WM_MOUSEMOVE:
				if (mouseDrag) {
					viewAngle += vec2(ivec2(msg.lParam & 0xFFFF, msg.lParam >> 16) - mouseOrig)*.005f;
					setMouse(POINT{ mouseOrig.x, mouseOrig.y });
				}
				break;
			case WM_LBUTTONUP:
				ShowCursor(true);
				mouseDrag = false;
				break;
			}
		}

		while (viewAngle.x > pi) viewAngle.x -= 2.f*pi;
		while (viewAngle.x < -pi) viewAngle.x += 2.f*pi;
		viewAngle.y = clamp(viewAngle.y, -.5*pi, .5*pi);

		mat4 cam = yRotate(viewAngle.x) * xRotate(-viewAngle.y);
		vec3 velocity(float(keyDown('D') - keyDown('A')), float(keyDown('E') - keyDown('Q')), float(keyDown('S') - keyDown('W')));
		cam.col(3) = camPosition += cam * vec4(velocity*.02f, .0f);
		auto icam = invert(cam);
		auto iproj = invert(proj);

		QueryPerformanceCounter(&current);
		float t = float(double(current.QuadPart - start.QuadPart) / double(frequency.QuadPart));

		float off = 5.f;

		TimeStamp start;

		glUseProgram(bookkeeper);
		glUniform1i("mode", 0);
		bindBuffer("hashIndices", hashIndices);
		bindBuffer("hashCounts", hashCounts);
		bindBuffer("hashOffsets", hashOffsets);
		glDispatchCompute(128, 1, 1);

		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		TimeStamp start2;
		glUseProgram(simulate);
		glUniform1i("mode", 2);
		glUniform1f("dt", .001f);
		bindBuffer("posBuffer", posBuffer[ping]);
		bindBuffer("velBuffer", velBuffer[ping]);
		bindBuffer("hashIndices", hashIndices);
		bindBuffer("hashCounts", hashCounts);
		bindBuffer("hashOffsets", hashOffsets);
		glDispatchCompute(128, 1, 1);

		TimeStamp start3;
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		glUseProgram(bookkeeper);
		glUniform1i("mode", 1);
		bindBuffer("hashIndices", hashIndices);
		bindBuffer("hashCounts", hashCounts);
		bindBuffer("hashOffsets", hashOffsets);
		glDispatchCompute(128, 1, 1);

		TimeStamp start4;
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		glUseProgram(simulate);
		glUniform1i("mode", 3);
		glUniform1f("dt", .001f);
		bindBuffer("posBuffer", posBuffer[ping]);
		bindBuffer("velBuffer", velBuffer[ping]);
		bindBuffer("hashIndices", hashIndices);
		bindBuffer("hashCounts", hashCounts);
		bindBuffer("hashOffsets", hashOffsets);
		glDispatchCompute(128, 1, 1);

		TimeStamp start5;
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		glUseProgram(simulate);
		glUniform1i("mode", 1);
		glUniform1f("dt", .001f);
		bindBuffer("posBuffer", posBuffer[ping]);
		bindBuffer("velBuffer", velBuffer[ping]);
		bindBuffer("posBufferOut", posBuffer[1 - ping]);
		bindBuffer("velBufferOut", velBuffer[1 - ping]);
		bindBuffer("hashIndices", hashIndices);
		bindBuffer("hashCounts", hashCounts);
		bindBuffer("hashOffsets", hashOffsets);
		glDispatchCompute(128, 1, 1);

		TimeStamp start6;
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		glUseProgram(drawParticles);
		bindBuffer("posBuffer", posBuffer[1 - ping]);
		bindBuffer("velBuffer", velBuffer[1 - ping]);
		glUniformMatrix4fv("worldToCamera", 1, false, icam.data);
		glUniformMatrix4fv("cameraToClip", 1, false, proj.data);
		glDrawArrays(GL_TRIANGLES, 0, N * 3);

		TimeStamp end;

		prevTime = elapsedTime(start6, end);
		font.drawText(
			std::to_wstring(elapsedTime(start, start2)) + L"\n" +
			std::to_wstring(elapsedTime(start2, start3)) + L"\n" +
			std::to_wstring(elapsedTime(start3, start4)) + L"\n" +
			std::to_wstring(elapsedTime(start4, start5)) + L"\n" +
			std::to_wstring(elapsedTime(start5, start6)) + L"\n" +
			std::to_wstring(prevTime) + L"ms", 5.f, 5.f, 15.f, screenw - 5);

		/*std::vector<uint> getter(N * 8);
		glGetNamedBufferSubData(hashCounts, 0, sizeof(uint)*N * 8, getter.data());
		int cumsum = 0; int maximum = 0, maxi = 0;
		for (int i = 0; i < getter.size(); ++i)
			if (getter[i] != 0) {
				if (getter[i] > maximum) {
					maximum = getter[i];
					maxi = i;
				}
				//printf("%d:%d\t(%d)\n", i, getter[i], cumsum += getter[i]);
			}
		printf("max %d at %d\n", maximum, maxi);*/
		swapBuffers();

		ping = 1 - ping;

		glClearColor(.0f, .0f, .0f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	return 0;
}


int main_bones() {

	OpenGL context(screenw, screenh, "", false);

	LARGE_INTEGER start, current, frequency;
	QueryPerformanceFrequency(&frequency);

	Font font(L"Consolas");

	bool loop = true;
	unsigned frame = 0;

	Texture<GL_TEXTURE_2D> result; glTextureStorage2D(result, 1, GL_RGBA16F, screenw, screenh);
	Texture<GL_TEXTURE_2D> nz; glTextureStorage2D(nz, 1, GL_RGBA16F, screenw, screenh);

	Program trace = createProgram("shaders/marchtrace.glsl");
	Program show = createProgram("shaders/marchShowVert.glsl", "", "", "", "shaders/marchShowFrag.glsl");
	Program boxshader = createProgram("shaders/boxVert.glsl", "", "", "", "shaders/boxFrag.glsl");



	mat4 identity(.0f);
	identity.diag() = vec4(1.f);
	float kek[] = { .0f, 1.f, .0f, 1.f, .0f, 1.f };
	std::cout << identity << std::endl;
	Buffer bones; glNamedBufferData(bones, sizeof(float) * 16, identity.data, GL_STREAM_DRAW);
	Buffer boxes; glNamedBufferData(boxes, sizeof(float) * 2 * 3, kek, GL_STREAM_DRAW);

	QueryPerformanceCounter(&start);
	float prevTime = .0f;

	vec4 camPosition(.0f, .0f, 0.f, 1.f);
	vec2 viewAngle(.0f, .0f);
	bool mouseDrag = false;
	ivec2 mouseOrig;
	ivec2 mouse;
	mat4 previousCam;
	mat4 proj = projection(screenw / float(screenh), 80.f, .2f, 15.0f);

	while (loop) {

		MSG msg;

		hitptr = 0;

		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			switch (msg.message) {
			case WM_QUIT:
				loop = false;
				break;
			case WM_LBUTTONDOWN:
				mouseDrag = true;
				mouseOrig = ivec2(msg.lParam & 0xFFFF, msg.lParam >> 16);
				ShowCursor(false);
				break;
			case WM_MOUSEMOVE:
				if (mouseDrag) {
					viewAngle += vec2(ivec2(msg.lParam & 0xFFFF, msg.lParam >> 16) - mouseOrig)*.005f;
					setMouse(POINT{ mouseOrig.x, mouseOrig.y });
				}
				break;
			case WM_LBUTTONUP:
				ShowCursor(true);
				mouseDrag = false;
				break;
			}
		}

		while (viewAngle.x > pi) viewAngle.x -= 2.f*pi;
		while (viewAngle.x < -pi) viewAngle.x += 2.f*pi;
		viewAngle.y = clamp(viewAngle.y, -.5*pi, .5*pi);

		mat4 cam = yRotate(viewAngle.x) * xRotate(-viewAngle.y);
		vec3 velocity(float(keyDown('D') - keyDown('A')), float(keyDown('E') - keyDown('Q')), float(keyDown('S') - keyDown('W')));
		cam.col(3) = camPosition += cam * vec4(velocity*.1f, .0f);
		auto icam = invert(cam);
		auto iproj = invert(proj);

		QueryPerformanceCounter(&current);
		float t = float(double(current.QuadPart - start.QuadPart) / double(frequency.QuadPart));

		float off = 5.f;

		TimeStamp start;

		glUseProgram(trace);
		bindImage("result", 0, result, GL_WRITE_ONLY, GL_RGBA16F);
		glUniform1ui("tick", GetTickCount());
		glUniformMatrix4fv("cameraToWorld", 1, false, cam.data);
		glUniformMatrix4fv("clipToCamera", 1, false, iproj.data);
		glDispatchCompute((screenw + 15) / 16, (screenh + 15) / 16, 1);

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		glDisable(GL_DEPTH_TEST);
		glUseProgram(show);
		bindTexture("tex", result);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		glClear(GL_DEPTH_BUFFER_BIT);

		glUseProgram(boxshader);

		bindBuffer("bones", bones);
		bindBuffer("extents", boxes);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);

		glUniformMatrix4fv("worldToCamera", 1, false, icam.data);
		glUniformMatrix4fv("camToClip", 1, false, proj.data);
		glDrawArrays(GL_TRIANGLES, 0, 36);


		/*
		for (int i = 0; i < downptr; ++i) {
			LPARAM scan = MapVirtualKeyEx(down[i], MAPVK_VK_TO_VSC_EX, GetKeyboardLayout(0));
			switch (down[i]) {
			case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
			case VK_RCONTROL: case VK_RMENU: case VK_LWIN: case VK_RWIN: case VK_APPS:
			case VK_PRIOR: case VK_NEXT: case VK_END: case VK_HOME: case VK_INSERT: case VK_DELETE:
			case VK_DIVIDE: case VK_NUMLOCK:
				scan |= KF_EXTENDED;
				break;
			default: break;
			}
			if (down[i] == VK_PAUSE) scan = 69; // MapVirtualKeyEx returns wrong code for pause (???)
			if (down[i] == VK_SEPARATOR) scan = 28|KF_EXTENDED;
			WCHAR name[256];
			GetKeyNameTextW(scan << 16, name, 256);
			std::wstring n(name);
			font.drawText(n, 10.f, off + 120.f, 15.f);
			off += 25.f;
		}


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
				L", r: " + std::to_wstring(r), 405.f, off, 15.f);
			off += 25.f;
		}*/

		TimeStamp end;

		prevTime = elapsedTime(start, end);
		font.drawText(std::to_wstring(prevTime) + L"ms", 5.f, 5.f, 15.f, screenw - 5);



		swapBuffers();
		glClearColor(.0f, .0f, .0f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	return 0;
}

void renderBigGuy(float t) {

	std::ifstream objFile("assets/bigguy.txt");
	//std::ifstream objFile("assets/monsterfrog.txt");

	std::vector<float> vertex, uv, normal; // vec4, vec2, vec4
	std::vector<int32_t> face; // 3 x ivec4 (pos, uv, normal)
	while (objFile.good()) {
		std::string line;
		std::getline(objFile, line, '\n');
		for (auto& a : line) if (a == '/') a = ' ';
		std::stringstream str(line);
		str >> line;
		if (!line.compare("v")) {
			float x, y, z;
			str >> x >> y >> z;
			vertex.push_back(x);
			vertex.push_back(y);
			vertex.push_back(z);
			vertex.push_back(1.f);
		}
		else if (!line.compare("vt")) {
			float u, v;
			str >> u >> v;
			uv.push_back(u);
			uv.push_back(v);
		}
		else if (!line.compare("vn")) {
			float x, y, z;
			str >> x >> y >> z;
			normal.push_back(x);
			normal.push_back(y);
			normal.push_back(z);
			normal.push_back(1.f);
		}
		else if (!line.compare("f")) {
			std::array<int32_t, 4> vi, ti, ni;
			for (int i = 0; i < 4; ++i)
				str >> vi[i] >> ti[i] >> ni[i];
			std::swap(vi[2], vi[3]);
			std::swap(ti[2], ti[3]);
			std::swap(ni[2], ni[3]);
			for (auto i : vi) face.push_back(i - 1);
			for (auto i : ti) face.push_back(i - 1);
			for (auto i : ni) face.push_back(i - 1);
		}
	}

	Buffer verts; glNamedBufferStorage(verts, sizeof(float)*vertex.size(), vertex.data(), 0);
	Buffer uvs; glNamedBufferStorage(uvs, sizeof(float)*uv.size(), uv.data(), 0);
	Buffer normals; glNamedBufferStorage(normals, sizeof(float)*normal.size(), normal.data(), 0);
	Buffer faces; glNamedBufferStorage(faces, sizeof(int32_t)*face.size(), face.data(), 0);

	Program quads = createProgram("shaders/quadVert.glsl", "", "", "", "shaders/quadFrag.glsl");

	glUseProgram(quads);

	float toClip[16];
	setupProjection(toClip, 1.f, float(screenw) / float(screenh), .1f, 200.f);

	glUniformMatrix4fv("toClip", 1, false, toClip);
	glUniform1f("t", t);

	bindBuffer("points", verts);
	bindBuffer("uvs", uvs);
	bindBuffer("normals", normals);
	bindBuffer("faces", faces);

	glDrawArrays(GL_TRIANGLES, 0, face.size());
}

static inline uint32_t sortable(float ff)
{
	uint32_t f = reinterpret_cast<float&>(ff);
	uint32_t mask = -int32_t(f >> 31) | 0x80000000;
	return f ^ mask;
}

static inline float unsortable(uint32_t f)
{
	uint32_t mask = ((f >> 31) - 1) | 0x80000000;
	f = f ^ mask;
	return reinterpret_cast<float&>(f);
}

int main_bvh() {

	OpenGL context(screenw, screenh, "", false);

	LARGE_INTEGER start, current, frequency;
	QueryPerformanceFrequency(&frequency);

	Buffer printBuffer = createPrintBuffer();

	Program splat = createProgram("shaders/splatVert.glsl", "", "", "", "shaders/splatFrag.glsl");
	Program text = createProgram("shaders/blitVert.glsl", "", "", "", "shaders/blitFrag.glsl");

	QueryPerformanceCounter(&start);
	bool loop = true;

	const int N = 1024 * 1024;
	const int dim = 3;

	Program init = createProgram("shaders/init.glsl");
	Program split = createProgram("shaders/split.glsl");
	Program cube = createProgram("shaders/cubeVert.glsl", "", "", "", "shaders/cubeFrag.glsl");

	struct PointCloud {
		Buffer points;
		Buffer extents;
		Buffer indices;
		Buffer nodes;
	} cloud[2];

	for (int i = 0; i < 2; ++i) {
		glNamedBufferStorage(cloud[i].points, sizeof(float) * 2 * dim * N, nullptr, 0);
		glNamedBufferStorage(cloud[i].extents, sizeof(float) * dim * N, nullptr, 0);
		glNamedBufferStorage(cloud[i].indices, sizeof(int) * 2 * N, nullptr, 0);
		glNamedBufferStorage(cloud[i].nodes, sizeof(int) * N, nullptr, 0);
	}

	Buffer buildExtents; glNamedBufferStorage(buildExtents, sizeof(float) * 2 * dim * (N - 1), nullptr, GL_DYNAMIC_STORAGE_BIT);
	Buffer buildIndices; glNamedBufferStorage(buildIndices, sizeof(int) * (1 + (N - 1)), nullptr, GL_DYNAMIC_STORAGE_BIT);
	Buffer buildSizes; glNamedBufferStorage(buildSizes, sizeof(int) * (N - 1), nullptr, GL_DYNAMIC_STORAGE_BIT);
	Buffer buildParents; glNamedBufferStorage(buildParents, sizeof(int) * (N - 1), nullptr, GL_DYNAMIC_STORAGE_BIT);

	Buffer threadCounter; glNamedBufferStorage(threadCounter, 2 * sizeof(int), nullptr, GL_DYNAMIC_STORAGE_BIT);

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

		glClearColor(.1f, .1f, .1f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		QueryPerformanceCounter(&current);
		float t = float(double(current.QuadPart - start.QuadPart) / double(frequency.QuadPart));

		float toWorld[16], toClip[16];

		lookAt(toWorld, .0f, 2.f, 2.f, .0f, .0f, .0f);
		setupProjection(toClip, 1.f, float(screenw) / float(screenh), .001f, 100.f);

		std::vector<uint32_t> val(dim * 2);

		int zero[2] = { 0 };
		glNamedBufferSubData(threadCounter, 0, sizeof(int) * 2, zero);
		glNamedBufferSubData(buildSizes, 0, sizeof(int), zero);

		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		glUseProgram(init);
		bindBuffer("points", cloud[0].points);
		bindBuffer("extents", cloud[0].extents);
		glUniform1i("N", N);
		glUniform1i("dim", dim);
		glUniform1f("t", t);
		glDispatchCompute(128, 1, 1);

		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		auto a = TimeStamp();

		glUseProgram(split);

		bindBuffer("points", cloud[0].points);
		bindBuffer("extents", cloud[0].extents);
		bindBuffer("indices", cloud[0].indices);
		bindBuffer("buildExtents", buildExtents);
		bindBuffer("buildIndices", buildIndices);
		bindBuffer("buildSizes", buildSizes);
		bindBuffer("buildParents", buildParents);
		bindBuffer("threadCounter", threadCounter);
		glUniform1i("N", N);
		glDispatchCompute((N + 255) / 256, 1, 1);

		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		auto b = TimeStamp();

		//glGetNamedBufferSubData(threadCounter, 0, sizeof(int) * 2, zero);
		//for (int i = 0; i < 3; ++i)
		//	glGetNamedBufferSubData(buildExtents, 2*i*N * sizeof(uint32_t), sizeof(uint32_t) * 2, val.data()+i*2);

		glUseProgram(cube);
		bindBuffer("points", cloud[0].points);
		bindBuffer("extents", cloud[0].extents);
		bindBuffer("buildExtents", buildExtents);
		bindBuffer("buildParents", buildParents);
		glUniform1i("N", N);
		glUniformMatrix4fv("toWorld", 1, false, toWorld);
		glUniformMatrix4fv("toClip", 1, false, toClip);
		glDrawArrays(GL_LINES, 0, N * 24 * 2);

		std::wstring kek = L"⏱: " + std::to_wstring(elapsedTime(a, b));// L"factory->CreateTextLayout\n✌️🧐\n⏱: " + std::to_wstring(prevElapsed) + L"ms\nps. " + std::to_wstring(renderer.points.size()*2*sizeof(float)) + L" BYTES\n🐎🐍🏋🌿🍆🔥👏\nThere is no going back. This quality of font rendering is the new norm 😂";

		swapBuffers();
	}
	return 0;
}

#include <random>
int main_trace() {

	OpenGL context(screenw, screenh, "", false);

	LARGE_INTEGER start, current, frequency;
	QueryPerformanceFrequency(&frequency);

	Buffer printBuffer = createPrintBuffer();

	Font font(L"Consolas");

	Program trace = createProgram("shaders/trace/trace.glsl");
	Program blit = createProgram("shaders/trace/blitVert.glsl", "", "", "", "shaders/trace/blitFrag.glsl");
	Program splat = createProgram("shaders/trace/splatVert.glsl", "", "", "", "shaders/trace/splatFrag.glsl");

	const int rndDimensions = 10;

	Matrix<float, rndDimensions, rndDimensions> A, Q, R;

	std::default_random_engine eng;

	for (int i = 0; i < rndDimensions; ++i)
		for (int j = 0; j < rndDimensions; ++j)
			A(i, j) = std::normal_distribution<float>()(eng);

	qr(A, Q, R);

	//auto K = Q.T() * Q;
	//std::cout << K << "\n";

	int counters[rndDimensions] = { 0 };

	int tentativeN = 5;
	int samples = 1024 * 1024;
	while (counters[rndDimensions - 1] < tentativeN) {
		for (int i = 0; i < rndDimensions; ++i) {
			counters[i]++;
			if (counters[i] >= tentativeN && i < rndDimensions - 1) counters[i] = 0;
			else break;
		}
	}

	Buffer colors; glNamedBufferStorage(colors, sizeof(float) * 4 * samples, nullptr, 0);
	Buffer coordinates; glNamedBufferStorage(coordinates, sizeof(float) * 2 * samples, nullptr, 0);

	std::vector<float> rnds(rndDimensions*samples);
	Buffer randoms; glNamedBufferStorage(randoms, sizeof(float) * rnds.size(), rnds.data(), 0);

	Framebuffer blitBuffer;
	Texture<GL_TEXTURE_2D> result; glTextureStorage2D(result, 1, GL_RGBA32F, screenw, screenh);

	auto tex = loadImage("assets/aalto.png");
	glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTextureParameteri(tex, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	bool loop = true;
	unsigned frame = 0;

	vec4 camPosition(-.5f, -.4f, 0.f, 1.f);
	vec2 viewAngle(pi / 2.f, .8f);
	vec3 velocityPositive(.0f), velocityNegative(.0f);
	bool mouseDrag = false;
	ivec2 mouseOrig;
	ivec2 mouse;
	mat4 previousCam;
	mat4 proj = projection(screenw / float(screenh), 120.f, .2f, 15.0f);

	QueryPerformanceCounter(&start);
	while (loop) {

		MSG msg;

		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			switch (msg.message) {
			case WM_LBUTTONDOWN:
				mouseDrag = true;
				mouseOrig = ivec2(msg.lParam & 0xFFFF, msg.lParam >> 16);
				ShowCursor(false);
				break;
			case WM_MOUSEMOVE:
				if (mouseDrag) {
					viewAngle += vec2(ivec2(msg.lParam & 0xFFFF, msg.lParam >> 16) - mouseOrig)*.005f;
					setMouse(POINT{ mouseOrig.x, mouseOrig.y });
				}
				break;
			case WM_LBUTTONUP:
				ShowCursor(true);
				mouseDrag = false;
				break;
			case WM_KEYDOWN: case WM_KEYUP:
				if (0 == (msg.lParam&(int(msg.message == WM_KEYDOWN) << 30))) {
					float vel = (msg.message == WM_KEYDOWN) ? 1.f : .0f;
					switch (msg.wParam) {
					case 'W': velocityNegative.z = vel; break;
					case 'S': velocityPositive.z = vel; break;
					case 'A': velocityNegative.x = vel; break;
					case 'D': velocityPositive.x = vel; break;
					case 'Q': velocityNegative.y = vel; break;
					case 'E': velocityPositive.y = vel; break;
					}
				}
				break;
			case WM_QUIT:
				loop = false;
				break;
			}
		}

		while (viewAngle.x > pi) viewAngle.x -= 2.f*pi;
		while (viewAngle.x < -pi) viewAngle.x += 2.f*pi;
		viewAngle.y = clamp(viewAngle.y, -.5*pi, .5*pi);

		mat4 cam = yRotate(viewAngle.x) * xRotate(-viewAngle.y);
		cam.col(3) = camPosition += cam * vec4((velocityPositive - velocityNegative)*.1f, .0f);

		if (cam != previousCam)
			frame = 0;

		if (!frame) {
			glBindFramebuffer(GL_FRAMEBUFFER, blitBuffer);
			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, result, 0);
			glDrawBuffer(GL_COLOR_ATTACHMENT0);
			glClear(GL_COLOR_BUFFER_BIT);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}


		auto icam = invert(cam);

		glClearColor(.1f, .1f, .1f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		QueryPerformanceCounter(&current);
		float t = float(double(current.QuadPart - start.QuadPart) / double(frequency.QuadPart));

		TimeStamp a;

		glUseProgram(trace);
		//bindPrintBuffer(trace, printBuffer);
		glUniform1i("rndDimensions", 0);
		bindBuffer("values", colors);
		bindBuffer("coordinates", coordinates);
		bindBuffer("randoms", randoms);
		glUniform1ui("frame", frame);
		glUniform1i("zero", 0);
		glUniform1ui("tick", int(current.QuadPart - start.QuadPart));
		glUniformMatrix4fv("cameraToWorld", 1, false, cam.data);
		glUniform2i("screenSize", screenw, screenh);
		bindTexture("emission", tex);
		glDispatchCompute((samples + 255) / 256, 1, 1);


		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);

		glUseProgram(splat);
		glBindFramebuffer(GL_FRAMEBUFFER, blitBuffer);
		bindOutputTexture("color", result, 0);

		bindBuffer("values", colors);
		bindBuffer("coordinates", coordinates);
		glUniform2i("screenSize", screenw, screenh);
		glUniform1i("frame", frame);
		glDrawArrays(GL_TRIANGLES, 0, 6 * samples);

		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		glDisable(GL_BLEND);

		glUseProgram(blit);
		glUniform1i("frame", frame);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		bindTexture("result", GL_TEXTURE_2D, result);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		TimeStamp b;

		font.drawText(L"⏱: " + std::to_wstring(elapsedTime(a, b)), 5.f, 5.f, 15.f, screenw - 10, screenh - 10);

		swapBuffers();

		frame++;
		previousCam = cam;
	}
	return 0;
}
