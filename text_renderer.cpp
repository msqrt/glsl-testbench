
#include "text_renderer.h"

#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "winmm.lib")

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

size_t TextRenderer::updateBuffers() {
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

HRESULT TextRenderer::DrawGlyphRun(void*, FLOAT baseX, FLOAT baseY, DWRITE_MEASURING_MODE mode, DWRITE_GLYPH_RUN const* run, DWRITE_GLYPH_RUN_DESCRIPTION const* runDesc, IUnknown*) {
	IDWriteColorGlyphRunEnumerator1* enumerator;

	std::vector<std::array<D2D1_POINT_2F, 4>> cubicSplines;
	std::vector<std::array<D2D1_POINT_2F, 3>> quadraticSplines;
	std::vector<std::array<D2D1_POINT_2F, 2>> lines;

	GeometrySink sink(cubicSplines, quadraticSplines, lines);

	HRESULT result = DWRITE_E_NOCOLOR;// factory->TranslateColorGlyphRun(D2D1_POINT_2F{ baseX, baseY }, run, runDesc, DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE | DWRITE_GLYPH_IMAGE_FORMATS_COLR | DWRITE_GLYPH_IMAGE_FORMATS_CFF, mode, nullptr, 0, &enumerator);
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
	/*else {
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
	}*/
	return S_OK;
}

// don't really care about these
HRESULT TextRenderer::DrawInlineObject(void* clientDrawingContext, FLOAT originX, FLOAT originY, IDWriteInlineObject *inlineObject, BOOL isSideways, BOOL isRightToLeft, IUnknown* clientDrawingEffect) { return S_OK; }
HRESULT TextRenderer::DrawStrikethrough(void *clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_STRIKETHROUGH const *strikethrough, IUnknown *clientDrawingEffect) { return S_OK; }
HRESULT TextRenderer::DrawUnderline(void *clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_UNDERLINE  const *strikethrough, IUnknown *clientDrawingEffect) { return S_OK; }

HRESULT TextRenderer::IsPixelSnappingDisabled(void*, BOOL* nope) {
	*nope = TRUE;
	return S_OK;
}
HRESULT TextRenderer::GetCurrentTransform(void*, DWRITE_MATRIX *m) {
	m->m11 = m->m22 = 1.f;
	m->m12 = m->m21 = m->dx = m->dy = .0f;
	return S_OK;
}
HRESULT TextRenderer::GetPixelsPerDip(void*, float* pixelsPerDip) {
	ID2D1Factory* d2dfactory;
	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dfactory);
	d2dfactory->ReloadSystemMetrics();
	float y;
	d2dfactory->GetDesktopDpi(nullptr, &y);
	*pixelsPerDip = y / 96.f;
	d2dfactory->Release();
	return S_OK;
}

HRESULT STDMETHODCALLTYPE TextRenderer::QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) {
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
ULONG STDMETHODCALLTYPE TextRenderer::AddRef(void) {
	return InterlockedIncrement(&counter);
}

ULONG STDMETHODCALLTYPE TextRenderer::Release(void) {
	ULONG result;
	if (0 == (result = InterlockedDecrement(&counter))) delete this;
	return result;
}

void Font::drawText(const std::wstring& text, float x, float y, float size, float maxwidth, float maxheight) {

	IDWriteTextFormat* format;
	//renderer.factory->CreateTextFormat(fontName.c_str(), nullptr, nullptr, 0, size, L"", &format);
	renderer.factory->CreateTextFormat(fontName.c_str(), nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"", &format);

	IDWriteTextLayout* layout;
	renderer.factory->CreateTextLayout(text.c_str(), text.length(), format, maxwidth, maxheight, (IDWriteTextLayout**)&layout);

	layout->Draw(nullptr, &renderer, x, y);
	size_t count = renderer.updateBuffers();

	if (count > 0) {

		// save some state
		bool depthTesting = glIsEnabled(GL_DEPTH_TEST);
		bool blending = glIsEnabled(GL_BLEND);

		GLenum orig_sfactor, orig_dfactor;
		glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint*)&orig_sfactor);
		glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint*)&orig_dfactor);

		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glUseProgram(program);

		GLint viewport[4];
		glGetIntegerv(GL_VIEWPORT, viewport);
		glUniform2f("screenSize", viewport[2]-viewport[0], viewport[3]-viewport[1]);

		bindBuffer("points", renderer.pointBuffer);
		bindBuffer("cols", renderer.colorBuffer);
		bindBuffer("inds", renderer.indexBuffer);
		bindBuffer("bounds", renderer.boundBuffer);
		bindBuffer("ranges", renderer.rangeBuffer);
		glUniform3f("textColor", 1.f, 1.f, 1.f);
		glDrawArrays(GL_TRIANGLES, 0, count * 6);

		if (depthTesting)
			glEnable(GL_DEPTH_TEST);
		if (!blending)
			glDisable(GL_BLEND);
		glBlendFunc(orig_sfactor, orig_dfactor);
	}

	format->Release();
	layout->Release();
}