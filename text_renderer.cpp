
#include "text_renderer.h"

#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "winmm.lib")

struct GeometrySink : public ID2D1GeometrySink {
	std::vector<std::array<D2D1_POINT_2F, 4>> cubicSplines;
	std::vector<std::array<D2D1_POINT_2F, 3>> quadraticSplines;
	std::vector<std::array<D2D1_POINT_2F, 2>> lines;

	D2D1_POINT_2F previousPoint, firstPoint;
	void AddBeziers(const D2D1_BEZIER_SEGMENT *points, UINT32 count) override {
		for (UINT32 i = 0; i < count; ++i)
			cubicSplines.push_back({ previousPoint, points[i].point1, points[i].point2, previousPoint = points[i].point3 });
	}
	void AddQuadraticBeziers(const D2D1_QUADRATIC_BEZIER_SEGMENT* points, UINT32 count) override {
		for (UINT32 i = 0; i < count; ++i)
			quadraticSplines.push_back({ previousPoint, points[i].point1, previousPoint = points[i].point2 });
	}
	void AddLines(const D2D1_POINT_2F *points, UINT32 count) override {
		for (UINT32 i = 0; i < count; ++i)
			lines.push_back({ previousPoint, previousPoint = points[i] });
	}
	void AddArc(const D2D1_ARC_SEGMENT* arc) override { printf("arcs not implemented!\n"); }
	void AddLine(const D2D1_POINT_2F point) override { AddLines(&point, 1); }
	void AddQuadraticBezier(const D2D1_QUADRATIC_BEZIER_SEGMENT *point) override { AddQuadraticBeziers(point, 1); }
	void AddBezier(const D2D1_BEZIER_SEGMENT *point) override { AddBeziers(point, 1); }
	void BeginFigure(D2D1_POINT_2F start, D2D1_FIGURE_BEGIN begin) override { firstPoint = previousPoint = start; }
	void EndFigure(D2D1_FIGURE_END end) override { if(end==D2D1_FIGURE_END_CLOSED) AddLines(&firstPoint, 1); }
	HRESULT Close() override { return S_OK; }
	void SetFillMode(D2D1_FILL_MODE fill) override {}
	void SetSegmentFlags(D2D1_PATH_SEGMENT flags) override {}

	// stores control points and returns the index ranges
	std::array<uint32_t, 4> EmitControlPoints(std::vector<D2D1_POINT_2F>& points) {

		std::array<uint32_t, 4> inds;
		inds[0] = (uint32_t)points.size();
		for (auto& a : cubicSplines) for (auto& p : a) points.push_back(p);
		
		inds[1] = (uint32_t)points.size();
		for (auto& a : quadraticSplines) for (auto& p : a) points.push_back(p);
		
		inds[2] = (uint32_t)points.size();
		for (auto& a : lines) for (auto& p : a) points.push_back(p);
		
		inds[3] = (uint32_t)points.size();
		return inds;
	}

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
	if (!glyphCacheValid) {
		glNamedBufferData(pointBuffer, points.size() * sizeof(D2D1_POINT_2F), points.data(), GL_STATIC_DRAW);
		glNamedBufferData(colorBuffer, colors.size() * sizeof(DWRITE_COLOR_F), colors.data(), GL_STATIC_DRAW);
		glNamedBufferData(indexBuffer, pointIndices.size() * 4 * sizeof(uint32_t), pointIndices.data(), GL_STATIC_DRAW);
		glyphCacheValid = true;
	}
	glNamedBufferData(boundBuffer, currentBounds.size() * 2 * sizeof(D2D1_POINT_2F), currentBounds.data(), GL_STREAM_DRAW);
	glNamedBufferData(rangeBuffer, currentRanges.size() * 2 * sizeof(uint32_t), currentRanges.data(), GL_STREAM_DRAW);
	size_t result = currentRanges.size();
	currentBounds.clear();
	currentRanges.clear();
	return result;
}

HRESULT TextRenderer::DrawGlyphRun(void*, FLOAT baseX, FLOAT baseY, DWRITE_MEASURING_MODE mode, DWRITE_GLYPH_RUN const* run, DWRITE_GLYPH_RUN_DESCRIPTION const* runDesc, IUnknown*) {
	
	HRESULT result = DWRITE_E_NOCOLOR;
	if (factory) {
		IDWriteColorGlyphRunEnumerator1* enumerator = nullptr;
		DWRITE_MATRIX transf = {.1f, .0f, .0f, .1f, .0f, .0f};
		result = factory->TranslateColorGlyphRun(D2D1_POINT_2F{ baseX, baseY }, run, runDesc, DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE | DWRITE_GLYPH_IMAGE_FORMATS_COLR | DWRITE_GLYPH_IMAGE_FORMATS_CFF, mode, &transf, 0, &enumerator);
		if (enumerator)
			enumerator->Release();
	}
	for (UINT32 i = 0; i < run->glyphCount; ++i) {
		UINT16 glyph = run->glyphIndices[i];
		auto glyphIterator = glyphToOffset.find(glyph);
		// glyph missing from cache
		if (glyphIterator == glyphToOffset.end()) {

			glyphIterator = glyphToOffset.insert(glyphIterator, std::make_pair(glyph, unsigned(colorRanges.size())));
			glyphCacheValid = false;

			if (result == DWRITE_E_NOCOLOR) {

				GeometrySink sink;
				run->fontFace->GetGlyphRunOutline(run->fontEmSize, &glyph, nullptr, nullptr, 1, run->isSideways, run->bidiLevel, &sink);

				std::array<uint32_t, 4> inds = sink.EmitControlPoints(points);

				// if we actually have geometry
				if (inds[3] > inds[0]) {
					D2D1_POINT_2F low, high;
					low = high = points[inds[0]];
					for (uint32_t j = inds[0] + 1; j < inds[3]; ++j) {
						D2D1_POINT_2F p = points[j];
						if (p.x < low.x) low.x = p.x;
						if (p.y < low.y) low.y = p.y;
						if (p.x > high.x) high.x = p.x;
						if (p.y > high.y) high.y = p.y;
					}
					for (UINT32 j = inds[0]; j < inds[3]; ++j) {
						points[j].x -= low.x;
						points[j].y -= low.y;
					}
					pointIndices.push_back(inds);
					colors.push_back(DWRITE_COLOR_F{ -1.0f, .0f, .0f, .0f });
					std::pair<unsigned, unsigned> range = std::make_pair(uint32_t(colors.size() - 1), uint32_t(colors.size()));

					boundingBoxes.push_back(std::make_pair(low, high));
					colorRanges.push_back(range);
				}
				else {
					boundingBoxes.push_back(std::make_pair(D2D1_POINT_2F{}, D2D1_POINT_2F{}));
					colorRanges.push_back(std::make_pair(0, 0));
				}
			}
			else {

				std::pair<unsigned, unsigned> range;
				range.first = uint32_t(colors.size());

				DWRITE_GLYPH_RUN tmpRun = *run;
				tmpRun.glyphIndices = &glyph;
				tmpRun.glyphCount = 1;

				IDWriteColorGlyphRunEnumerator1* enumerator;
				factory->TranslateColorGlyphRun(D2D1_POINT_2F{ baseX, baseY }, &tmpRun, runDesc, DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE | DWRITE_GLYPH_IMAGE_FORMATS_COLR | DWRITE_GLYPH_IMAGE_FORMATS_CFF, mode, nullptr, 0, &enumerator);

				while (true) {

					BOOL hasRun;
					enumerator->MoveNext(&hasRun);
					if (!hasRun) break;

					DWRITE_COLOR_GLYPH_RUN1 const* colorRun;
					enumerator->GetCurrentRun(&colorRun);
					if (colorRun->runColor.a == .0f) continue; // full alpha? probably not a thing

					colors.push_back(colorRun->runColor);

					GeometrySink sink;
					colorRun->glyphRun.fontFace->GetGlyphRunOutline(colorRun->glyphRun.fontEmSize, colorRun->glyphRun.glyphIndices, colorRun->glyphRun.glyphAdvances, colorRun->glyphRun.glyphOffsets, colorRun->glyphRun.glyphCount, colorRun->glyphRun.isSideways, colorRun->glyphRun.bidiLevel, &sink);
					pointIndices.push_back(sink.EmitControlPoints(points));
				}

				enumerator->Release();

				range.second = uint32_t(colors.size());

				D2D1_POINT_2F low, high;
				low = high = points[pointIndices[range.first][0]];
				for (uint32_t j = pointIndices[range.first][0] + 1; j < pointIndices[range.second - 1][3]; ++j) {
					D2D1_POINT_2F p = points[j];
					if (p.x < low.x) low.x = p.x;
					if (p.y < low.y) low.y = p.y;
					if (p.x > high.x) high.x = p.x;
					if (p.y > high.y) high.y = p.y;
				}
				for (uint32_t j = pointIndices[range.first][0]; j < pointIndices[range.second - 1][3]; ++j) {
					points[j].x -= low.x;
					points[j].y -= low.y;
				}

				glyphCacheValid = false;

				boundingBoxes.push_back(std::make_pair(low, high));
				colorRanges.push_back(range);
			}
		}

		unsigned offset = glyphIterator->second;
		std::pair<unsigned, unsigned> range = colorRanges[offset];

		if (range.second > range.first) {
			D2D1_POINT_2F low, high;
			low = boundingBoxes[offset].first;
			high = boundingBoxes[offset].second;

			low.x += baseX; low.y += baseY;
			high.x += baseX; high.y += baseY;

			// these add the location+glyph to the actually rendered list
			currentBounds.push_back(std::make_pair(low, high));
			currentRanges.push_back(range);
		}
		if (run->glyphAdvances)
			baseX += run->glyphAdvances[i];
	}
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

extern HWND wnd;
HRESULT TextRenderer::GetPixelsPerDip(void*, float* pixelsPerDip) {
	ID2D1Factory* d2dfactory;
	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dfactory);
	d2dfactory->ReloadSystemMetrics();
	*pixelsPerDip = float(GetDpiForWindow(wnd)) / 96.f;
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

ULONG STDMETHODCALLTYPE TextRenderer::AddRef(void) {
	return InterlockedIncrement(&counter);
}

ULONG STDMETHODCALLTYPE TextRenderer::Release(void) {
	ULONG result;
	if (0 == (result = InterlockedDecrement(&counter))) delete this;
	return result;
}

void Font::drawText(const std::wstring& text, float x, float y, float size, std::array<float,3> color, float maxwidth, float maxheight) {

	if (renderer.factory) {
		IDWriteTextFormat3* format;
		renderer.factory->CreateTextFormat(fontName.c_str(), nullptr, nullptr, 0, size, L"", &format);

		IDWriteTextLayout4* layout;
		renderer.factory->CreateTextLayout(text.c_str(), (UINT32)text.length(), format, maxwidth, maxheight, (IDWriteTextLayout**)&layout);

		layout->Draw(nullptr, &renderer, x, y);

		format->Release();
		layout->Release();
	}
	else {
		IDWriteTextFormat* format;
		renderer.backupFactory->CreateTextFormat(fontName.c_str(), nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"", &format);
		IDWriteTextLayout* layout;
		renderer.backupFactory->CreateTextLayout(text.c_str(), (UINT32)text.length(), format, maxwidth, maxheight, &layout);

		layout->Draw(nullptr, &renderer, x, y);

		format->Release();
		layout->Release();
	}

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
		glUniform2f("screenSize", float(viewport[2]-viewport[0]), float(viewport[3]-viewport[1]));

		bindBuffer("points", renderer.pointBuffer);
		bindBuffer("cols", renderer.colorBuffer);
		bindBuffer("inds", renderer.indexBuffer);
		bindBuffer("bounds", renderer.boundBuffer);
		bindBuffer("ranges", renderer.rangeBuffer);
		glUniform3fv("textColor", 1, color.data());
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)count * 6);

		if (depthTesting)
			glEnable(GL_DEPTH_TEST);
		if (!blending)
			glDisable(GL_BLEND);
		glBlendFunc(orig_sfactor, orig_dfactor);
	}
}