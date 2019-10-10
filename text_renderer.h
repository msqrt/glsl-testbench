
#pragma once

#include <string>
#include <dwrite_3.h>
#include <d2d1_3.h>
#include <vector>
#include <map>
#include <array>
#include "gl_helpers.h"

// too bad GDI doesn't support emoji 😂 gotta deal with DW
struct TextRenderer : public IDWriteTextRenderer {

	IDWriteFactory7* factory = nullptr;
	IDWriteFactory* backupFactory = nullptr;
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

	size_t updateBuffers();

	HRESULT DrawGlyphRun(void*, FLOAT baseX, FLOAT baseY, DWRITE_MEASURING_MODE mode, DWRITE_GLYPH_RUN const* run, DWRITE_GLYPH_RUN_DESCRIPTION const* runDesc, IUnknown*) override;
	
	// don't really care about these
	HRESULT DrawInlineObject(void* clientDrawingContext, FLOAT originX, FLOAT originY, IDWriteInlineObject *inlineObject, BOOL isSideways, BOOL isRightToLeft, IUnknown* clientDrawingEffect);
	HRESULT DrawStrikethrough(void *clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_STRIKETHROUGH const *strikethrough, IUnknown *clientDrawingEffect);
	HRESULT DrawUnderline(void *clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY, DWRITE_UNDERLINE  const *strikethrough, IUnknown *clientDrawingEffect);

	HRESULT IsPixelSnappingDisabled(void*, BOOL* nope) override;
	HRESULT GetCurrentTransform(void*, DWRITE_MATRIX *m) override;
	HRESULT GetPixelsPerDip(void*, float* pixelsPerDip) override;

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) override;

	ULONG counter = 1;
	ULONG STDMETHODCALLTYPE AddRef(void) override;
	ULONG STDMETHODCALLTYPE Release(void) override;
};

struct Font {
	Font(const std::wstring& name) : fontName(name) {
		if(S_OK != DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory7), reinterpret_cast<IUnknown**>(&renderer.factory)))
			DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&renderer.backupFactory));
		program = createProgram("shaders/textVert.glsl", "", "", "", "shaders/textFrag.glsl");
	}
	~Font() {
		if(renderer.factory)
			renderer.factory->Release();
		if (renderer.backupFactory)
			renderer.backupFactory->Release();
	}
	void drawText(const std::wstring& text, float x, float y, float size, float maxwidth = 1e8f, float maxheight = 1e8f);
private:
	std::wstring fontName;
	TextRenderer renderer;
	Program program;
};
