
#include <windows.h>
#include "window.h"
#include <dxgi1_6.h>
#include <physicalmonitorenumerationapi.h>
#include <vector>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxva2.lib")

HWND wnd;
ID3D12Device5* device = nullptr;
ID3D12CommandQueue* queue = nullptr;
IDXGISwapChain1* chain = nullptr;
IDXGIFactory7* factory = nullptr;

// update the frame onto screen
void swapBuffers() {
	DXGI_PRESENT_PARAMETERS presentParameters = { 0, nullptr, nullptr, nullptr }; // whole screen
	chain->Present1(0, DXGI_PRESENT_RESTART, &presentParameters);
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
		if (!keyDown(wParam))
			down[downptr++] = hit[hitptr++] = wParam;
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
		for (int i = 0; i < downptr - 1; ++i)
			if (wParam == down[i]) {
				down[i] = down[downptr-1];
				break;
			}
		if (downptr > 0) downptr--;
		return 0;
	case WM_KILLFOCUS:
		hitptr = downptr = 0;
		return 0;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
/*
void APIENTRY glDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar * message, void *) {

	// format the message nicely
	auto output = std::string("GLDEBUG: OpenGL ");

	switch (type) {
	case GL_DEBUG_TYPE_ERROR:					output += "error";							break;
	case GL_DEBUG_TYPE_PORTABILITY:				output += "portability issue";				break;
	case GL_DEBUG_TYPE_PERFORMANCE:				output += "performance issue";				break;
	case GL_DEBUG_TYPE_OTHER:					output += "issue";							break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:		output += "undefined behavior";				break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:		output += "deprecated behavior";			break;
	default:									output += "unknown issue";					break;
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
*/

std::wstring getMonitorName(HMONITOR monitor) {
	MONITORINFOEXW info;
	info.cbSize = sizeof(info);
	GetMonitorInfoW(monitor, &info);

	UINT32 requiredPaths, requiredModes;
	GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, &requiredModes);
	std::vector<DISPLAYCONFIG_PATH_INFO> paths(requiredPaths);
	std::vector<DISPLAYCONFIG_MODE_INFO> modes(requiredModes);
	QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, paths.data(), &requiredModes, modes.data(), nullptr);

	for (auto& p : paths) {
		DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName;
		sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
		sourceName.header.size = sizeof(sourceName);
		sourceName.header.adapterId = p.sourceInfo.adapterId;
		sourceName.header.id = p.sourceInfo.id;
		DisplayConfigGetDeviceInfo(&sourceName.header);
		if (wcscmp(info.szDevice, sourceName.viewGdiDeviceName) == 0) {
			DISPLAYCONFIG_TARGET_DEVICE_NAME name;
			name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
			name.header.size = sizeof(name);
			name.header.adapterId = p.sourceInfo.adapterId;
			name.header.id = p.targetInfo.id;
			DisplayConfigGetDeviceInfo(&name.header);
			return std::wstring(name.monitorFriendlyDeviceName);
		}
	}
}

void setupD3D() {
	CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, __uuidof(IDXGIFactory7), (void**)&factory);

	IDXGIAdapter4* adapter;
	for (int i = 0; factory->EnumAdapters1(i, (IDXGIAdapter1**)&adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC3 adapterDesc;
		adapter->GetDesc3(&adapterDesc);
		if (adapterDesc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE) continue; // not interested
		printf("device %d:\n%ls\n%d %d MB VRAM\n", i, adapterDesc.Description, adapterDesc.DedicatedVideoMemory / (1024 * 1024 * 1000), (adapterDesc.DedicatedVideoMemory / (1024 * 1024)) % 1000);
		IDXGIOutput6* output;
		for (int j = 0; adapter->EnumOutputs(j, (IDXGIOutput**)&output) != DXGI_ERROR_NOT_FOUND; ++j) {
			DXGI_OUTPUT_DESC1 outputDesc;
			output->GetDesc1(&outputDesc);
			int width = outputDesc.DesktopCoordinates.right - outputDesc.DesktopCoordinates.left,
				height = outputDesc.DesktopCoordinates.bottom - outputDesc.DesktopCoordinates.top;

			printf("%ls, %dx%d\n", getMonitorName(outputDesc.Monitor).c_str(), width, height);
		}
	}

	factory->EnumAdapters1(0, (IDXGIAdapter1**)&adapter);

	ID3D12Debug1* debug;
	D3D12GetDebugInterface(__uuidof(debug), (void**)&debug);
	debug->SetEnableGPUBasedValidation(true);
	debug->Release();

	D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, __uuidof(ID3D12Device5), (void**)&device);

	D3D12_COMMAND_QUEUE_DESC queueDesc;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT
	queueDesc.NodeMask = 0;
	device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void**)&queue);
}

void closeD3D() {
	if (queue) queue->Release();
	queue = nullptr;
	if (factory) factory->Release();
	factory = nullptr;
	if (device) device->Release();
	device = nullptr;
}

void setupWindow(int width, int height, const std::string& title, bool fullscreen) {

	WNDCLASSEX windowClass = { 0 };
	windowClass.cbSize = sizeof(windowClass);
	windowClass.hInstance = GetModuleHandle(nullptr);
	windowClass.lpszClassName = TEXT("classy class");
	windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	windowClass.style = CS_OWNDC;
	windowClass.lpfnWndProc = wndProc;
	RegisterClassEx(&windowClass);

	DXGI_SWAP_CHAIN_DESC1 swapDesc = { 0 };
	swapDesc.Width = width; swapDesc.Height = height;
	swapDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	swapDesc.Stereo = false;
	swapDesc.SampleDesc.Count = 1;
	swapDesc.SampleDesc.Quality = 0;
	swapDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.BufferCount = 2;
	swapDesc.Scaling = DXGI_SCALING_STRETCH;
	swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreenDesc = { 0 };
	fullscreenDesc.RefreshRate.Numerator = 60;
	fullscreenDesc.RefreshRate.Denominator = 1;
	fullscreenDesc.Scaling = DXGI_MODE_SCALING_STRETCHED;
	fullscreenDesc.Windowed = !fullscreen;

	// adjust the window borders away, center the window
	RECT area = { 0, 0, width, height };
	DWORD style = fullscreen ? WS_POPUP : (WS_SYSMENU|WS_CAPTION|WS_MINIMIZEBOX);

	AdjustWindowRect(&area, style, false);
	int adjustedWidth = fullscreen ? width : area.right - area.left, adjustedHeight = fullscreen ? height : area.bottom - area.top;
	int centerX = (GetSystemMetrics(SM_CXSCREEN) - adjustedWidth) / 2, centerY = (GetSystemMetrics(SM_CYSCREEN) - adjustedHeight) / 2;
	if (fullscreen)
		centerX = centerY = 0;

	// create the final window and context
	wnd = CreateWindowA("classy class", title.c_str(), style, centerX, centerY, adjustedWidth, adjustedHeight, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
	
	factory->CreateSwapChainForHwnd(queue, wnd, &swapDesc, &fullscreenDesc, nullptr, &chain);

	ShowWindow(wnd, SW_SHOW);
}

void closeWindow() {
	// release context, window
	if(chain) chain->Release();
	chain = nullptr;
	ChangeDisplaySettings(nullptr, CDS_FULLSCREEN);
	DestroyWindow(wnd);
	UnregisterClass(TEXT("classy class"), GetModuleHandle(nullptr));
}
