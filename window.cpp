
#include <windows.h>
#include "window.h"
#include <dxgi1_6.h>
#include <physicalmonitorenumerationapi.h>
#include <vector>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxva2.lib")

HWND wnd;
IDXGIFactory7* factory = nullptr;
ID3D12Device5* device = nullptr;
IDXGISwapChain4* chain = nullptr;
ID3D12CommandQueue* queue = nullptr;

ID3D12GraphicsCommandList* list = nullptr;
ID3D12Fence1* fence = nullptr;

ID3D12CommandAllocator* allocator = nullptr;

ID3D12RootSignature *graphicsRoot = nullptr, *computeRoot = nullptr;

// update the frame onto screen
void swapBuffers() {

	UINT count;
	chain->GetLastPresentCount(&count);

	if (fence->GetCompletedValue() != count) {
		HANDLE fenceEvent = CreateEvent(nullptr, 0, 0, nullptr);
		fence->SetEventOnCompletion(count, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
		CloseHandle(fenceEvent);
	}

	allocator->Reset();
	list->Reset(allocator, nullptr);

	ID3D12Resource1* backBuffer;
	chain->GetBuffer(chain->GetCurrentBackBufferIndex(), __uuidof(ID3D12Resource1), (void**)&backBuffer);

	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = backBuffer;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	list->ResourceBarrier(1, &barrier);


	D3D12_RESOURCE_DESC texDesc = backBuffer->GetDesc();

	D3D12_VIEWPORT port;
	port.TopLeftX = 0;
	port.TopLeftY = 0;
	port.Width = texDesc.Width;
	port.Height = texDesc.Height;
	port.MinDepth = .0f;
	port.MaxDepth = 1.f;

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
	heapDesc.NumDescriptors = 1;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NodeMask = 0;

	ID3D12DescriptorHeap* heap;
	device->CreateDescriptorHeap(&heapDesc, __uuidof(ID3D12DescriptorHeap), (void**)&heap);


	D3D12_CPU_DESCRIPTOR_HANDLE view = heap->GetCPUDescriptorHandleForHeapStart();

	D3D12_RENDER_TARGET_VIEW_DESC viewDesc;
	viewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	viewDesc.Texture2D.MipSlice = 0;
	viewDesc.Texture2D.PlaneSlice = 0;
	device->CreateRenderTargetView(backBuffer, &viewDesc, view);

	list->OMSetRenderTargets(1, &view, 0, nullptr);

	const float color[] = { .1f, .1f, .1f, .0f };
	list->ClearRenderTargetView(view, color, 0, nullptr);

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

	list->ResourceBarrier(1, &barrier);
	list->Close();

	heap->Release();
	backBuffer->Release();

	queue->ExecuteCommandLists(1, (ID3D12CommandList**)&list);
	chain->Present(0, DXGI_PRESENT_RESTART);
	queue->Signal(fence, count+1);
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
	case WM_WINDOWPOSCHANGED:
		if (((WINDOWPOS*)lParam)->hwnd == wnd) {
			if (chain) {
				//chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
				return 0;
			}
		}
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
	D3D12GetDebugInterface(__uuidof(ID3D12Debug1), (void**)&debug);
	debug->EnableDebugLayer();
	//debug->SetEnableGPUBasedValidation(true); // potentially slow
	debug->Release();

	D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, __uuidof(ID3D12Device5), (void**)&device);

	D3D12_COMMAND_QUEUE_DESC queueDesc;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT
	queueDesc.NodeMask = 0;
	device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void**)&queue);

	device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence1), (void**)&fence);
	fence->Signal(0);

	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&allocator);
	
	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, __uuidof(ID3D12GraphicsCommandList), (void**)&list);
	list->Close();
}

void closeD3D() {
	if (fence) {
		HANDLE fenceEvent = CreateEvent(nullptr, 0, 0, nullptr);
		queue->Signal(fence, 1);
		if (fence->GetCompletedValue() < 1) {
			fence->SetEventOnCompletion(1, fenceEvent);
			WaitForSingleObject(fenceEvent, INFINITE);
		}
		CloseHandle(fenceEvent);
		fence->Release();
		fence = nullptr;
	}
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
	swapDesc.Width = width;
	swapDesc.Height = height;
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
	fullscreenDesc.Windowed = true;

	// adjust the window borders away, center the window
	RECT area = { 0, 0, width, height };
	DWORD style = WS_SYSMENU|WS_CAPTION|WS_MINIMIZEBOX|WS_MAXIMIZEBOX;

	AdjustWindowRect(&area, style, false);
	int adjustedWidth = area.right - area.left, adjustedHeight = area.bottom - area.top;
	int centerX = (GetSystemMetrics(SM_CXSCREEN) - adjustedWidth) / 2, centerY = (GetSystemMetrics(SM_CYSCREEN) - adjustedHeight) / 2;

	// create the final window and context
	wnd = CreateWindowA("classy class", title.c_str(), style, centerX, centerY, adjustedWidth, adjustedHeight, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
	
	factory->CreateSwapChainForHwnd(queue, wnd, &swapDesc, &fullscreenDesc, nullptr, (IDXGISwapChain1**)&chain);

	if (fullscreen)
		chain->SetFullscreenState(true, nullptr);

	ShowWindow(wnd, SW_SHOW);
}

void closeWindow() {
	// release context, window
	if (chain) {
		chain->SetFullscreenState(false, nullptr);
		UINT count;
		chain->GetLastPresentCount(&count);
		if (fence->GetCompletedValue() != count) {
			HANDLE fenceEvent = CreateEvent(nullptr, 0, 0, nullptr);
			fence->SetEventOnCompletion(count, fenceEvent);
			WaitForSingleObject(fenceEvent, INFINITE);
			CloseHandle(fenceEvent);
		}
		chain->Release();
		chain = nullptr;
	}
	DestroyWindow(wnd);
	UnregisterClass(TEXT("classy class"), GetModuleHandle(nullptr));
}
