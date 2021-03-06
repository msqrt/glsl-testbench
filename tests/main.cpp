
#include "window.h"
#include "shaderprintf.h"
#include <dwrite_3.h>
#include <d2d1_3.h>
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d2d1.lib")

#include <vector>
#include <map>
#include <array>

//const int screenw = 1600, screenh = 900;

const int resultSize = 512;
const int screenw = resultSize * 3, screenh = resultSize;
//const int screenw = 600, screenh = 600;

IDWriteFactory7* factory;

struct GeometrySink : public ID2D1GeometrySink {
	std::vector<std::array<D2D1_POINT_2F, 4>>& cubicSplines;
	std::vector<std::array<D2D1_POINT_2F, 3>>& quadraticSplines;
	std::vector<std::array<D2D1_POINT_2F, 2>>& lines;

	GeometrySink(
		std::vector<std::array<D2D1_POINT_2F, 4>>& cubicSplines,
		std::vector<std::array<D2D1_POINT_2F, 3>>& quadraticSplines,
		std::vector<std::array<D2D1_POINT_2F, 2>>& lines) :
		cubicSplines(cubicSplines),
		quadraticSplines(quadraticSplines),
		lines(lines)
	{ }

	D2D1_POINT_2F previous;
	void AddBeziers(const D2D1_BEZIER_SEGMENT *points, UINT32 count) override {
		for (int i = 0; i < count; ++i)
			cubicSplines.push_back({ previous, points[i].point1, points[i].point2, previous = points[i].point3 });
	}
	void AddQuadraticBeziers(const D2D1_QUADRATIC_BEZIER_SEGMENT* points, UINT32 count) override {
		for (int i = 0; i < count; ++i)
			quadraticSplines.push_back({ previous, points[i].point1, previous = points[i].point2 });
	}
	void AddLines(const D2D1_POINT_2F *points, UINT32 count) override {
		for (int i = 0; i < count; ++i)
			lines.push_back({ previous, previous = points[i] });
	}
	void AddArc(const D2D1_ARC_SEGMENT* arc) override { printf("arcs not implemented!\n"); }
	void AddLine(const D2D1_POINT_2F point) override { AddLines(&point, 1); }
	void AddQuadraticBezier(const D2D1_QUADRATIC_BEZIER_SEGMENT *point) override { AddQuadraticBeziers(point, 1); }
	void AddBezier(const D2D1_BEZIER_SEGMENT *point) override { AddBeziers(point, 1); }
	D2D1_POINT_2F beginPoint;
	void BeginFigure(D2D1_POINT_2F start, D2D1_FIGURE_BEGIN begin) override { beginPoint = previous = start; }
	void EndFigure(D2D1_FIGURE_END end) override { AddLines(&beginPoint, 1); }
	HRESULT Close() override { return S_OK; }
	void SetFillMode(D2D1_FILL_MODE fill) override {}
	void SetSegmentFlags(D2D1_PATH_SEGMENT flags) override {}
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) override {
		if (!ppvObject) return E_INVALIDARG;
		*ppvObject = NULL;
		if (riid == __uuidof(IUnknown) || riid == __uuidof(ID2D1SimplifiedGeometrySink) || riid == __uuidof(ID2D1GeometrySink)) {
			*ppvObject = (LPVOID)this;
			AddRef();
			return S_OK;
		}
		return E_NOINTERFACE;
	}
	ULONG counter = 1;
	ULONG STDMETHODCALLTYPE AddRef(void) override {
		return InterlockedIncrement(&counter);
	}
	ULONG STDMETHODCALLTYPE Release(void) override {
		ULONG result;
		if (0 == (result = InterlockedDecrement(&counter))) delete this;
		return result;
	}
};

struct TextRenderer : public IDWriteTextRenderer {

	std::vector<D2D1_POINT_2F> points;

	// these are added in pairs; pointIndices refers to the elements to be rendered with the current color
	std::vector<std::array<uint32_t, 4>> pointIndices;
	std::vector<DWRITE_COLOR_F> colors;

	// these are added in pairs; colorRanges refers to the pointIndices/colors lists
	std::vector<std::pair<D2D1_POINT_2F, D2D1_POINT_2F>> boundingBoxes;
	std::vector<std::pair<uint32_t, uint32_t>> colorRanges;

	// this is an index to the colorRanges buffer
	std::map<std::pair<float, UINT16>, unsigned> glyphToOffset;

	// copied from boundingboxes/colorRanges
	std::vector<std::pair<D2D1_POINT_2F, D2D1_POINT_2F>> currentBounds;
	std::vector<std::pair<uint32_t, uint32_t>> currentRanges;

	bool valid = false;

	Buffer pointBuffer, colorBuffer, indexBuffer, boundBuffer, rangeBuffer;

	size_t updateBuffers() {
		if (!valid) {
			glNamedBufferData(pointBuffer, points.size() * sizeof(D2D1_POINT_2F), points.data(), GL_STATIC_DRAW);
			glNamedBufferData(colorBuffer, colors.size() * sizeof(DWRITE_COLOR_F), colors.data(), GL_STATIC_DRAW);
			glNamedBufferData(indexBuffer, pointIndices.size() * 4 * sizeof(uint32_t), pointIndices.data(), GL_STATIC_DRAW);
			valid = true;
		}
		glNamedBufferData(boundBuffer, currentBounds.size() * 2 * sizeof(D2D1_POINT_2F), currentBounds.data(), GL_STREAM_DRAW);
		glNamedBufferData(rangeBuffer, currentRanges.size() * 2 * sizeof(uint32_t), currentRanges.data(), GL_STREAM_DRAW);
		size_t result = currentRanges.size();
		currentBounds.clear();
		currentRanges.clear();
		return result;
	}

	HRESULT DrawGlyphRun(void*, FLOAT baseX, FLOAT baseY, DWRITE_MEASURING_MODE mode, DWRITE_GLYPH_RUN const* run, DWRITE_GLYPH_RUN_DESCRIPTION const* runDesc, IUnknown*) override {
		IDWriteColorGlyphRunEnumerator1* enumerator;

		std::vector<std::array<D2D1_POINT_2F, 4>> cubicSplines;
		std::vector<std::array<D2D1_POINT_2F, 3>> quadraticSplines;
		std::vector<std::array<D2D1_POINT_2F, 2>> lines;

		GeometrySink sink(cubicSplines, quadraticSplines, lines);

		HRESULT result = factory->TranslateColorGlyphRun(D2D1_POINT_2F{ baseX, baseY }, run, runDesc, DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE | DWRITE_GLYPH_IMAGE_FORMATS_COLR | DWRITE_GLYPH_IMAGE_FORMATS_CFF, mode, nullptr, 0, &enumerator);
		if (result == DWRITE_E_NOCOLOR) {
			for (int i = 0; i < run->glyphCount; ++i) {
				UINT16 glyph = run->glyphIndices[i];
				auto glyphIterator = glyphToOffset.find(std::make_pair(run->fontEmSize, glyph));
				std::pair<unsigned, unsigned> range;
				D2D1_POINT_2F low, high;
				if (glyphIterator == glyphToOffset.end()) {
					//printf("%d, %g\n", run->glyphIndices[i], baseX);
					glyphIterator = glyphToOffset.insert(glyphIterator, std::make_pair(std::make_pair(run->fontEmSize, glyph), unsigned(colorRanges.size())));

					cubicSplines.clear();
					quadraticSplines.clear();
					lines.clear();

					run->fontFace->GetGlyphRunOutline(run->fontEmSize, &glyph, nullptr, nullptr, 1, run->isSideways, run->bidiLevel, &sink);

					std::array<uint32_t, 4> inds;
					inds[0] = (uint32_t)points.size();
					for (auto& a : cubicSplines) for (auto& p : a) points.push_back(p);
					inds[1] = (uint32_t)points.size();
					for (auto& a : quadraticSplines) for (auto& p : a) points.push_back(p);
					inds[2] = (uint32_t)points.size();
					for (auto& a : lines) for (auto& p : a) points.push_back(p);
					inds[3] = (uint32_t)points.size();

					if (inds[3] > inds[0]) {
						low = high = points[inds[0]];
						for (int j = inds[0] + 1; j < inds[3]; ++j) {
							D2D1_POINT_2F p = points[j];
							if (p.x < low.x) low.x = p.x;
							if (p.y < low.y) low.y = p.y;
							if (p.x > high.x) high.x = p.x;
							if (p.y > high.y) high.y = p.y;
						}
						for (int j = inds[0]; j < inds[3]; ++j) {
							points[j].x -= low.x;
							points[j].y -= low.y;
						}
						pointIndices.push_back(inds);
						colors.push_back(DWRITE_COLOR_F{ -1.0f, .0f, .0f, .0f });
						valid = false;
						range = std::make_pair(uint32_t(colors.size() - 1), uint32_t(colors.size()));
					}
					else {
						low.x = low.y = high.x = high.y = .0f;
						range = std::make_pair(uint32_t(colors.size()), uint32_t(colors.size()));
					}
					boundingBoxes.push_back(std::make_pair(low, high));
					colorRanges.push_back(range);
				}
				else {
					unsigned offset = glyphIterator->second;
					low = boundingBoxes[offset].first;
					high = boundingBoxes[offset].second;
					range = colorRanges[offset];
				}
				low.x += baseX; low.y += baseY;
				high.x += baseX; high.y += baseY;
				if (range.second > range.first) {
					currentBounds.push_back(std::make_pair(low, high));
					currentRanges.push_back(range);
				}
				if (run->glyphAdvances)
					baseX += run->glyphAdvances[i];
			}
		}
		else {
			enumerator->Release();
			for (int i = 0; i < run->glyphCount; ++i) {
				UINT16 glyph = run->glyphIndices[i];
				auto glyphIterator = glyphToOffset.find(std::make_pair(run->fontEmSize, glyph));

				std::pair<unsigned, unsigned> range;
				D2D1_POINT_2F low, high;
				if (glyphIterator == glyphToOffset.end()) {

					glyphIterator = glyphToOffset.insert(glyphIterator, std::make_pair(std::make_pair(run->fontEmSize, glyph), unsigned(colorRanges.size())));

					range.first = colors.size();

					DWRITE_GLYPH_RUN tmpRun = *run;
					tmpRun.glyphIndices = &glyph;
					tmpRun.glyphCount = 1;

					factory->TranslateColorGlyphRun(D2D1_POINT_2F{ baseX, baseY }, &tmpRun, runDesc, DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE | DWRITE_GLYPH_IMAGE_FORMATS_COLR | DWRITE_GLYPH_IMAGE_FORMATS_CFF, mode, nullptr, 0, &enumerator);
					//printf("SNIB\n");
					BOOL hasRun = TRUE;
					//enumerator->MoveNext(&hasRun);
					//enumerator->MoveNext(&hasRun);

					while (true) {

						enumerator->MoveNext(&hasRun);
						if (!hasRun) break;
						DWRITE_COLOR_GLYPH_RUN1 const* colorRun;
						enumerator->GetCurrentRun(&colorRun);
						if (colorRun->runColor.a == .0f) continue;

						colors.push_back(colorRun->runColor);

						cubicSplines.clear();
						quadraticSplines.clear();
						lines.clear();

						auto desc = colorRun->glyphRunDescription;

						colorRun->glyphRun.fontFace->GetGlyphRunOutline(colorRun->glyphRun.fontEmSize, colorRun->glyphRun.glyphIndices, colorRun->glyphRun.glyphAdvances, colorRun->glyphRun.glyphOffsets, colorRun->glyphRun.glyphCount, colorRun->glyphRun.isSideways, colorRun->glyphRun.bidiLevel, &sink);

						std::array<uint32_t, 4> inds;
						inds[0] = (uint32_t)points.size();
						for (auto& a : cubicSplines) for (auto& p : a) points.push_back(p);
						inds[1] = (uint32_t)points.size();
						for (auto& a : quadraticSplines) for (auto& p : a) points.push_back(p);
						inds[2] = (uint32_t)points.size();
						for (auto& a : lines) for (auto& p : a) points.push_back(p);
						inds[3] = (uint32_t)points.size();

						pointIndices.push_back(inds);
					}

					enumerator->Release();

					//printf("SNAB\n");
					range.second = colors.size();

					low = high = points[pointIndices[range.first][0]];
					for (int j = pointIndices[range.first][0] + 1; j < pointIndices[range.second - 1][3]; ++j) {
						D2D1_POINT_2F p = points[j];
						if (p.x < low.x) low.x = p.x;
						if (p.y < low.y) low.y = p.y;
						if (p.x > high.x) high.x = p.x;
						if (p.y > high.y) high.y = p.y;
					}
					for (int j = pointIndices[range.first][0]; j < pointIndices[range.second - 1][3]; ++j) {
						points[j].x -= low.x;
						points[j].y -= low.y;
					}

					valid = false;

					boundingBoxes.push_back(std::make_pair(low, high));
					colorRanges.push_back(range);

				}
				else {
					unsigned offset = glyphIterator->second;
					low = boundingBoxes[offset].first;
					high = boundingBoxes[offset].second;
					range = colorRanges[offset];
				}
				low.x += baseX; low.y += baseY;
				high.x += baseX; high.y += baseY;
				if (range.second > range.first) {
					currentBounds.push_back(std::make_pair(low, high));
					currentRanges.push_back(range);
				}
				if (run->glyphAdvances)
					baseX += run->glyphAdvances[i]; // let's hope the bw and color fonts have the same advances
			}
		}
		return S_OK;
	}

	// don't really care about these
	HRESULT DrawInlineObject(void* clientDrawingContext, FLOAT originX, FLOAT originY, IDWriteInlineObject *inlineObject, BOOL isSideways, BOOL isRightToLeft, IUnknown* clientDrawingEffect) override { return S_OK; }
	HRESULT DrawStrikethrough(void *clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_STRIKETHROUGH const *strikethrough, IUnknown *clientDrawingEffect) override { return S_OK; }
	HRESULT DrawUnderline(void *clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_UNDERLINE  const *strikethrough, IUnknown *clientDrawingEffect) override { return S_OK; }

	HRESULT IsPixelSnappingDisabled(void*, BOOL* nope) override {
		*nope = TRUE;
		return S_OK;
	}
	HRESULT GetCurrentTransform(void*, DWRITE_MATRIX *m) override {
		m->m11 = m->m22 = 1.f;
		m->m12 = m->m21 = m->dx = m->dy = .0f;
		return S_OK;
	}
	HRESULT GetPixelsPerDip(void*, float* pixelsPerDip) override {
		ID2D1Factory* d2dfactory;
		D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dfactory);
		d2dfactory->ReloadSystemMetrics();
		float y;
		d2dfactory->GetDesktopDpi(nullptr, &y);
		*pixelsPerDip = y / 96.f;
		d2dfactory->Release();
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) override {
		if (!ppvObject)
			return E_INVALIDARG;
		*ppvObject = NULL;
		if (riid == __uuidof(IUnknown) || riid == __uuidof(IDWriteTextRenderer) || riid == __uuidof(IDWritePixelSnapping)) {
			*ppvObject = (LPVOID)this;
			AddRef();
			return S_OK;
		}
		return E_NOINTERFACE;
	}

	ULONG counter = 1;
	ULONG STDMETHODCALLTYPE AddRef(void) override {
		return InterlockedIncrement(&counter);
	}

	ULONG STDMETHODCALLTYPE Release(void) override {
		ULONG result;
		if (0 == (result = InterlockedDecrement(&counter))) delete this;
		return result;
	}
};
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

int main() {

	OpenGL context(screenw, screenh, "", false);

	LARGE_INTEGER start, current, frequency;
	QueryPerformanceCounter(&start);
	QueryPerformanceFrequency(&frequency);

	bool loop = true;

	Buffer printBuffer = createPrintBuffer();

	// too bad GDI doesn't support emoji 😂 gotta deal with DW
	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory4), reinterpret_cast<IUnknown**>(&factory));

	Program splat = createProgram("shaders/splatVert.glsl", "", "", "", "shaders/splatFrag.glsl");
	Program text = createProgram("shaders/blitVert.glsl", "", "", "", "shaders/blitFrag.glsl");

	Program trace = createProgram("shaders/trace.glsl");
	Program convolution = createProgram("shaders/convolution.glsl");
	Program blit = createProgram("shaders/finalVert.glsl", "", "", "", "shaders/finalFrag.glsl");
	TextRenderer renderer;

	const int inputFeatures = 10; const int outputFeatures = 3; const int splatKernelSize = 4; const int upscaleKernelSize = 4;
	const int featureMultiply = 3; const int levels = 3;

	Texture<GL_TEXTURE_2D_ARRAY> inputImage, targetImage;
	glTextureStorage3D(inputImage, 1, GL_R16F, resultSize, resultSize, inputFeatures); // color, normal, first reflection dir, depth
	glTextureStorage3D(targetImage, 1, GL_R16F, resultSize, resultSize, outputFeatures);

	/*
	for any count of samples:
	§1 generate samples to inputImage
	§2 run samples through a matrix to generate weights for kernels, max with 0
	§3 splat kernels with given weights to different levels of a mip pyramid
	to reconstruct image:
	§1 run each pixel in mip pyramid through a (level-dependent) matrix and max with 0
	§2 splat kernels to lower level, sum with approppriate layer, max with 0
	*/

	int features[levels];
	features[0] = outputFeatures; // final image will have three channels

	Texture<GL_TEXTURE_2D> firstTransform[2];
	Texture<GL_TEXTURE_2D_ARRAY> splatKernels[2];
	Texture<GL_TEXTURE_2D_ARRAY> upscaleKernels[2];

	Texture<GL_TEXTURE_2D_ARRAY> splatPyramid[levels], decodePyramid[levels];
	int res[levels];
	res[0] = resultSize;
	int totalFeatures[levels+1];
	totalFeatures[0] = 0;
	for (int i = 0; i < levels; ++i) {
		if (i > 0) features[i] = features[i - 1] * featureMultiply;
		glTextureStorage3D(splatPyramid[i], 1, GL_R16F, res[i], res[i], features[i]);
		glTextureStorage3D(decodePyramid[i], 1, GL_R16F, res[i], res[i], features[i]);
		if (i < levels - 1)
			res[i + 1] = res[i] / 4;
		if (i > 0)
			totalFeatures[i] = totalFeatures[i - 1] + features[i-1];
		printf("%d features\n", features[i]);
	}
	totalFeatures[levels] = totalFeatures[levels-1] + features[levels-1];

	for (int k = 0; k < 2; ++k) {
		glTextureStorage2D(firstTransform[k], 1, GL_R16F, inputFeatures, totalFeatures[levels]);
		glTextureStorage3D(splatKernels[k], 1, GL_R16F, splatKernelSize, splatKernelSize, totalFeatures[levels]);
		glTextureStorage3D(upscaleKernels[k], 1, GL_R16F, upscaleKernelSize, upscaleKernelSize, totalFeatures[levels]);
	}

	Program randomize = createProgram("shaders/randomize.glsl");
	Program splatSamples = createProgram("shaders/sampleSplatVert.glsl", "", "", "shaders/sampleSplatGeom.glsl", "shaders/sampleSplatFrag.glsl");
	Program clear = createProgram("shaders/clearVert.glsl", "", "", "shaders/clearGeom.glsl", "shaders/clearFrag.glsl");
	Program upscale = createProgram("shaders/upscale.glsl");
	Program difference = createProgram("shaders/difference.glsl");

	Buffer diff;

	float prevElapsed = .0f;

	Framebuffer fbo;

	float prevBest = FLT_MAX;

	int frame = 0;
	float rate = 1.f;
	int betterCounter = 0;
	float avgError = .0f;
	int optimizeIndex = 0;
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

		auto a = TimeStamp();

		glUseProgram(trace);

		glUniform1i("frame", frame * 2);
		glUniform1i("scene", frame);
		glUniform1i("renderAux", 1);
		bindImage("result", 0, inputImage, GL_WRITE_ONLY, GL_R16F);

		glDispatchCompute((resultSize + 15) / 16, (resultSize + 15) / 16, 1);

		glUniform1i("frame", frame * 2 + 1);
		glUniform1i("renderAux", 0);
		bindImage("result", 0, targetImage, GL_WRITE_ONLY, GL_R16F);

		glDispatchCompute((resultSize + 15) / 16, (resultSize + 15) / 16, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		const int sampleCount = resultSize * resultSize;
		if (betterCounter == 0) {
			glUseProgram(randomize);
			glUniform1i("frame", frame);
			glUniform1f("rate", rate);
			bindImage("splat", 0, splatKernels[optimizeIndex], GL_WRITE_ONLY, GL_R16F);
			bindImage("upscale", 0, upscaleKernels[optimizeIndex], GL_WRITE_ONLY, GL_R16F);
			bindImage("matrix", 0, firstTransform[optimizeIndex], GL_WRITE_ONLY, GL_R16F);
			bindImage("baseSplat", 0, splatKernels[1 - optimizeIndex], GL_READ_WRITE, GL_R16F);
			bindImage("baseUpscale", 0, upscaleKernels[1 - optimizeIndex], GL_READ_WRITE, GL_R16F);
			bindImage("baseMatrix", 0, firstTransform[1 - optimizeIndex], GL_READ_WRITE, GL_R16F);
			int sizey = max(splatKernelSize, upscaleKernelSize);
			int sizex = max(sizey, inputFeatures);
			glDispatchCompute((sizex + 7) / 8, (sizey + 7) / 8, totalFeatures[levels]);
		}
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glUseProgram(clear);
		for (int i = 0; i < levels; ++i) {
			bindOutputTexture("outFeature", splatPyramid[i], 0);
			glDrawArrays(GL_POINTS, 0, features[i]);
		}
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		glUseProgram(splatSamples);
		bindImage("inputImage", 0, inputImage, GL_READ_ONLY, GL_R16F);
		bindImage("splat", 0, splatKernels[optimizeIndex], GL_READ_ONLY, GL_R16F);
		bindTexture("kernel", GL_TEXTURE_2D_ARRAY, splatKernels[optimizeIndex]);
		bindImage("matrix", 0, firstTransform[optimizeIndex], GL_READ_ONLY, GL_R16F);
		for (int i = 0; i < levels; ++i) {
			glUniform1i("res", res[i]);
			glUniform1i("features", features[i]);
			glUniform1i("featureOffset", totalFeatures[i]);
			bindOutputTexture("outFeature", splatPyramid[i], 0);
			glDrawArrays(GL_TRIANGLES, 0, sampleCount * features[i] * 6);
		}
		glDisable(GL_BLEND);
		/*
		glUseProgram(upscale);
		bindImage("kernel", 0, upscaleKernels[optimizeIndex], GL_READ_ONLY, GL_R16F);
		for (int i = levels - 2; i >= 0; --i) {
			bindImage("upper", 0, splatPyramid[i + 1], GL_READ_ONLY, GL_R16F);
			bindImage("result", 0, splatPyramid[i], GL_WRITE_ONLY, GL_R16F);
			glDispatchCompute((res[i] + 15) / 16, (res[i] + 15) / 16, features[i]);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		}
		*/
		glMemoryBarrier(GL_ALL_BARRIER_BITS);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, screenw, screenh);
		glUseProgram(blit);
		bindImage("noisy", 0, inputImage, GL_READ_ONLY, GL_R16F);
		bindImage("result", 0, splatPyramid[0], GL_READ_ONLY, GL_R16F);
		bindImage("target", 0, targetImage, GL_READ_ONLY, GL_R16F);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		auto b = TimeStamp();
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		float zero[4] = { .0f };
		glNamedBufferData(diff, sizeof(float), zero, GL_STREAM_DRAW);
		glUseProgram(difference);
		bindBuffer("difference", diff);
		bindImage("image", 0, splatPyramid[0], GL_READ_ONLY, GL_R16F);
		bindImage("target", 0, targetImage, GL_READ_ONLY, GL_R16F);
		glDispatchCompute(128, 1, 1);
		glMemoryBarrier(GL_ALL_BARRIER_BITS);
		
		glGetNamedBufferSubData(diff, 0, sizeof(float), zero);

		if (zero[0] < prevBest) {
			++betterCounter;
			avgError += zero[0];
			if (betterCounter > 20) {
				optimizeIndex = 1 - optimizeIndex;
				prevBest = avgError / float(betterCounter);
				betterCounter = 0;
				rate *= .999f;
			}
		}
		else {
			avgError = .0f;
			betterCounter = 0;
		}

		IDWriteTextFormat3* format;
		factory->CreateTextFormat(L"Consolas", nullptr, nullptr, 0, 15.f, L"", &format);

		IDWriteTextLayout4* layout; // 😂 I hate my life 😂\n top kek right bros???\n Λ, λ 🧐 👌 
		std::wstring kek = L"avg: " + std::to_wstring(prevBest) + L" vs " + std::to_wstring(zero[0]) + L"\n⏱: " + std::to_wstring(elapsedTime(a, b)) + L"\nframe #" + std::to_wstring(frame);// L"factory->CreateTextLayout\n✌️🧐\n⏱: " + std::to_wstring(prevElapsed) + L"ms\nps. " + std::to_wstring(renderer.points.size()*2*sizeof(float)) + L" BYTES\n🐎🐍🏋🌿🍆🔥👏\nThere is no going back. This quality of font rendering is the new norm 😂";
		factory->CreateTextLayout(kek.c_str(), kek.length(), format, screenw - 10, screenh - 10, (IDWriteTextLayout**)&layout);

		layout->Draw(nullptr, &renderer, 5.0f, 5.0f);
		size_t count = renderer.updateBuffers();

		if (count > 0) {
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glUseProgram(text);

			bindBuffer("points", renderer.pointBuffer);
			bindBuffer("cols", renderer.colorBuffer);
			bindBuffer("inds", renderer.indexBuffer);
			bindBuffer("bounds", renderer.boundBuffer);
			bindBuffer("ranges", renderer.rangeBuffer);
			glUniform2f("screenSize", screenw, screenh);
			glUniform3f("textColor", 1.f, 1.f, 1.f);
			glDrawArrays(GL_TRIANGLES, 0, count * 6);
			glDisable(GL_BLEND);
			glEnable(GL_DEPTH_TEST);
		}

		format->Release();
		layout->Release();

		swapBuffers();
		Sleep(200);
		frame++;
	}
	factory->Release();
	return 0;
}
