
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
