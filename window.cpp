
#include <windows.h>
#include "window.h"

#include "shaderprintf.h"

#pragma comment(lib, "opengl32.lib")

const int GL_WINDOW_ATTRIBUTES[] = {
	WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
	WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
	WGL_DOUBLE_BUFFER_ARB, GL_TRUE,

	WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
	WGL_COLOR_BITS_ARB, 32,
	WGL_DEPTH_BITS_ARB, 24,
0 };

const int GL_CONTEXT_ATTRIBUTES[] = {
	WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
	WGL_CONTEXT_MINOR_VERSION_ARB, 6,
	WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
	//WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
0 };

HWND wnd = nullptr;
HDC dc;
HGLRC rc;

//HHOOK globalKeyHook;

// update the frame onto screen
void swapBuffers() {
	SwapBuffers(dc);
}

void setTitle(const std::string& title) {
	SetWindowTextA(wnd, title.c_str());
}

POINT getMouse() {
	POINT mouse;
	GetCursorPos(&mouse);
	ScreenToClient(wnd, &mouse);
	return mouse;
}

void setMouse(POINT p) {
	ClientToScreen(wnd, &p);
	SetCursorPos(p.x, p.y);
}

WPARAM down[256]; int downptr = 0;
WPARAM hit[256]; int hitptr = 0;

bool keyDown(UINT vk_code) {
	for (int i = 0; i < downptr; ++i)
		if (vk_code == down[i])
			return true;
	return false;
}

bool keyHit(UINT vk_code) {
	for (int i = 0; i < hitptr; ++i)
		if (vk_code == hit[i])
			return true;
	return false;
}

void resetHits() {
	hitptr = 0;
}

inline WPARAM mapExtended(WPARAM wParam, LPARAM lParam) {
	int ext = (lParam>>24)&1;
	switch(wParam) {
		case VK_SHIFT: return MapVirtualKeyEx((lParam >> 16) & 0xFF, MAPVK_VSC_TO_VK_EX, GetKeyboardLayout(0));
		case VK_CONTROL: return VK_LCONTROL+ext;
		case VK_MENU: return VK_LMENU+ext;
		case VK_RETURN: return VK_RETURN + ext*(VK_SEPARATOR-VK_RETURN);
		default: return wParam;
	}
}

std::wstring text;
// only handle quitting here and do the rest in the message loop
LRESULT CALLBACK wndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
	case WM_CHAR:
		if(wParam == VK_BACK) // backspace
			text = text.substr(0, text.length() - 1);
		else
			text += wchar_t(wParam);
		return 0;
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
		wParam = mapExtended(wParam, lParam);
		if (!keyDown((UINT)wParam))
			down[downptr++] = hit[hitptr++] = wParam; // todo: if mapNarrowed(wParam)!=wParam: also add mapNarrowed()
		if (wParam == 'V' && (keyDown(VK_LCONTROL) || keyDown(VK_RCONTROL))) {
			if (OpenClipboard(nullptr)) {
				HANDLE data = GetClipboardData(CF_UNICODETEXT);
				if (data) {
					wchar_t* clipText = (wchar_t*)GlobalLock(data);
					if (clipText) {
						text += std::wstring(clipText);
						text = text.substr(0, text.length());
						GlobalUnlock(data);
					}
				}
				CloseClipboard();
			}
		}
		if (wParam == VK_ESCAPE)
			PostQuitMessage(0);
		return 0;
	case WM_SYSKEYUP:
	case WM_KEYUP:
		wParam = mapExtended(wParam, lParam);
		for (int i = 0; i < downptr - 1; ++i) // todo: if mapNarrowed(wParam)!=wParam: also remove mapNarrowed()
			if (wParam == down[i]) {
				down[i] = down[downptr-1];
				break;
			}
		if (downptr > 0) downptr--;
		return 0;
	case WM_KILLFOCUS:
	//	hitptr = downptr = 0;
		return 0;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

/*LRESULT CALLBACK CallWndProc(
	_In_ int    nCode,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam )
{
	if(nCode>=0){
		auto* p = (CWPSTRUCT*)lParam;
		switch (p->message) {
		case WM_SYSKEYDOWN: case WM_KEYDOWN: case WM_SYSKEYUP: case WM_KEYUP:
			wndProc(p->hwnd, p->message, p->wParam, p->lParam);
		}
	}
	return CallNextHookEx(nullptr, nCode, wParam, lParam);
}*/


bool loop() {
	hitptr = 0;
	MSG msg;
	bool result = true;
	while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		switch (msg.message) {
			case WM_QUIT:
				result = false;
				break;
		}
	}
	return result;
}

void APIENTRY glDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar * message, void *) {

	// format the message nicely
	auto output = std::string("OpenGL ");

	switch (type) {
	case GL_DEBUG_TYPE_ERROR:					output += "error";							break;
	case GL_DEBUG_TYPE_PORTABILITY:				output += "portability issue";				break;
	case GL_DEBUG_TYPE_PERFORMANCE:				output += "performance issue";				break;
	case GL_DEBUG_TYPE_OTHER:					output += "issue";							break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:		output += "undefined behavior";				break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:		output += "deprecated behavior";			break;
	default:									output += "issue(?)";						break;
	}
	switch (source) {
	case GL_DEBUG_SOURCE_API:																break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:			output += " in the window system";			break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER:		output += " in the shader compiler";		break;
	case GL_DEBUG_SOURCE_THIRD_PARTY:			output += " in third party code";			break;
	case GL_DEBUG_SOURCE_APPLICATION:			output += " in this program";				break;
	case GL_DEBUG_SOURCE_OTHER:					output += " in an undefined source";		break;
	default:									output += " nowhere(?)";					break;
	}

	output += ", id " + std::to_string(id) + ":\n";
	output += std::string(message, length);

	printf("%s\n", output.c_str());

	// this is invaluable; you can directly see where any opengl error originated by checking the call stack at this breakpoint
	if (type == GL_DEBUG_TYPE_ERROR)
		_CrtDbgBreak();
}

GLuint vertexArray;
GLenum* drawBuffers;

void setupGL(int width, int height, const std::string& title, bool fullscreen, bool show) {

	if (glOpen()) return;

	WNDCLASSEX windowClass = { 0 };
	windowClass.cbSize = sizeof(windowClass);
	windowClass.hInstance = GetModuleHandle(nullptr);
	windowClass.lpszClassName = TEXT("classy class");
	windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	windowClass.style = CS_OWNDC;
	windowClass.lpfnWndProc = wndProc;
	RegisterClassEx(&windowClass);

	PIXELFORMATDESCRIPTOR formatDesc = { 0 };
	formatDesc.nVersion = 1;
	formatDesc.nSize = sizeof(formatDesc);
	formatDesc.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
	formatDesc.iLayerType = PFD_MAIN_PLANE;
	formatDesc.iPixelType = PFD_TYPE_RGBA;
	formatDesc.cColorBits = 32;
	formatDesc.cDepthBits = 24;
	formatDesc.cStencilBits = 8;

	// create a temporary window to have a functional opengl context in order to get some extension function pointers
	HWND tempWnd = CreateWindow(TEXT("classy class"), TEXT("windy window"), WS_POPUP, 0, 0, width, height, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
	HDC tempDC = GetDC(tempWnd);
	SetPixelFormat(tempDC, ChoosePixelFormat(tempDC, &formatDesc), &formatDesc);
	HGLRC tempRC = wglCreateContext(tempDC);
	wglMakeCurrent(tempDC, tempRC);

	// these are why we made the temporary context; can set more pixel format attributes (multisample, floating point etc.) and create debug/core/etc contexts
	auto wglChoosePixelFormat = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
	auto wglCreateContextAttribs = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

	// adjust the window borders away, center the window
	RECT area = { 0, 0, width, height };
	const DWORD style = (fullscreen ? WS_POPUP : (WS_SYSMENU|WS_CAPTION|WS_MINIMIZEBOX)) | ((fullscreen||show)?WS_VISIBLE:0);
	if (fullscreen) {
		DEVMODE mode = { 0 };
		mode.dmSize = sizeof(mode);
		mode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
		mode.dmPelsWidth = width; mode.dmPelsHeight = height;
		mode.dmBitsPerPel = 32;
		ChangeDisplaySettings(&mode, CDS_FULLSCREEN);
	}

	AdjustWindowRect(&area, style, false);
	int adjustedWidth = fullscreen ? width : area.right - area.left, adjustedHeight = fullscreen ? height : area.bottom - area.top;
	int centerX = (GetSystemMetrics(SM_CXSCREEN) - adjustedWidth) / 2, centerY = (GetSystemMetrics(SM_CYSCREEN) - adjustedHeight) / 2;
	if (fullscreen)
		centerX = centerY = 0;

	// create the final window and context
	dc = GetDC(wnd = CreateWindowA("classy class", title.c_str(), style, centerX, centerY, adjustedWidth, adjustedHeight, nullptr, nullptr, GetModuleHandle(nullptr), nullptr));

	int format; UINT numFormats;
	wglChoosePixelFormat(dc, GL_WINDOW_ATTRIBUTES, nullptr, 1, &format, &numFormats);

	SetPixelFormat(dc, format, &formatDesc);

	rc = wglCreateContextAttribs(dc, 0, GL_CONTEXT_ATTRIBUTES);

	wglMakeCurrent(dc, rc);
	glViewport(0, 0, width, height);

	// release the temporary window and context
	wglDeleteContext(tempRC);
	ReleaseDC(tempWnd, tempDC);
	DestroyWindow(tempWnd);

	// what GL did we get?
	printf("gl  %s\non  %s\nby  %s\nsl  %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER), glGetString(GL_VENDOR), glGetString(GL_SHADING_LANGUAGE_VERSION));

	loadgl();

	// enable debug output
	glEnable(GL_DEBUG_OUTPUT);
	// debug output from main thread: step-by-step debug from the correct stack frame when breaking
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

	glDebugMessageCallback((GLDEBUGPROC)glDebugCallback, 0);

	// query everything about errors, deprecated and undefined things
	glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_ERROR, GL_DONT_CARE, 0, 0, GL_TRUE);
	glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR, GL_DONT_CARE, 0, 0, GL_TRUE);
	glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR, GL_DONT_CARE, 0, 0, GL_TRUE);

	// disable misc info. might want to check these from time to time!
	glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_OTHER, GL_DONT_CARE, 0, 0, GL_FALSE);
	glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_PERFORMANCE, GL_DONT_CARE, 0, 0, GL_FALSE);

	// we won't be using VBOs so a single global VAO is plenty
	glCreateVertexArrays(1, &vertexArray);
	glBindVertexArray(vertexArray);

	glEnable(GL_FRAMEBUFFER_SRGB);

	//globalKeyHook = SetWindowsHookExA(WH_CALLWNDPROC, CallWndProc, nullptr, 0);
}

void closeGL() {
	if (!glOpen()) return;

	//UnhookWindowsHookEx(globalKeyHook);

	glBindVertexArray(0);
	glDeleteVertexArrays(1, &vertexArray);

	// release context, window
	ChangeDisplaySettings(nullptr, CDS_FULLSCREEN);
	wglMakeCurrent(0, 0);
	wglDeleteContext(rc);
	ReleaseDC(wnd, dc);
	DestroyWindow(wnd);
	wnd = nullptr;
	UnregisterClass(TEXT("classy class"), GetModuleHandle(nullptr));
	_CrtDumpMemoryLeaks();
}

bool glOpen() {
	return wnd != nullptr;
}

void showWindow() {
	ShowWindow(wnd, SW_SHOW);
}

void hideWindow() {
	ShowWindow(wnd, SW_HIDE);
}
