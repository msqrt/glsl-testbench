
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
0 };

const int GL_CONTEXT_ATTRIBUTES[] = {
	WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
	WGL_CONTEXT_MINOR_VERSION_ARB, 5,
	WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
	WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
0 };

HWND wnd;
HDC dc;
HGLRC rc;

// update the frame onto screen
void swapBuffers() {
	SwapBuffers(dc);
}

void setTitle(const std::string& title) {
	SetWindowText(wnd, title.c_str());
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

// only handle quitting here and do the rest in the message loop
LRESULT CALLBACK wndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (uMsg == WM_CLOSE || uMsg == WM_KEYDOWN && wParam == VK_ESCAPE)
		PostQuitMessage(0);
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

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

	// this is invaluable; you can directly see where any opengl error originated by stepping forward from this breakpoint
	if (type == GL_DEBUG_TYPE_ERROR)
		_CrtDbgBreak();
}

GLuint vertexArray;
int bufferCount;
GLenum* drawBuffers;

void setupGL(int width, int height, const std::string& title, bool fullscreen) {

	WNDCLASSEX windowClass = { 0 };
	windowClass.cbSize = sizeof(windowClass);
	windowClass.hInstance = GetModuleHandle(nullptr);
	windowClass.lpszClassName = "classy class";
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
	HWND tempWnd = CreateWindow("classy class", "windy window", WS_POPUP, 0, 0, width, height, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
	HDC tempDC = GetDC(tempWnd);
	SetPixelFormat(tempDC, ChoosePixelFormat(tempDC, &formatDesc), &formatDesc);
	HGLRC tempRC = wglCreateContext(tempDC);
	wglMakeCurrent(tempDC, tempRC);

	// these are why we made the temporary context; can set more pixel format attributes (multisample, floating point etc.) and create debug/core/etc contexts
	auto wglChoosePixelFormat = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
	auto wglCreateContextAttribs = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

	// adjust the window borders away, center the window
	RECT area = { 0, 0, width, height };
	DWORD style = (fullscreen ? WS_POPUP : (WS_SYSMENU|WS_CAPTION|WS_MINIMIZEBOX)) | WS_VISIBLE;
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
	dc = GetDC(wnd = CreateWindow("classy class", title.c_str(), style, centerX, centerY, adjustedWidth, adjustedHeight, nullptr, nullptr, GetModuleHandle(nullptr), nullptr));

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

	// to keep track of bound fbo buffers
	glGetIntegerv(GL_MAX_DRAW_BUFFERS, &bufferCount);
	drawBuffers = new GLenum[bufferCount];
}

void closeGL() {
	glBindVertexArray(0);
	glDeleteVertexArrays(1, &vertexArray);

	delete[] drawBuffers;

	// release context, window
	ChangeDisplaySettings(nullptr, CDS_FULLSCREEN);
	wglMakeCurrent(0, 0);
	wglDeleteContext(rc);
	ReleaseDC(wnd, dc);
	DestroyWindow(wnd);
	UnregisterClass("classy class", GetModuleHandle(nullptr));
	_CrtDumpMemoryLeaks();
}

#include <fstream>
#include <sstream>

// create a single shader object
GLuint createShader(const std::string& path, GLenum shaderType) {
	using namespace std;
	// read file, set as source
	string source = string(istreambuf_iterator<char>(ifstream(path).rdbuf()), istreambuf_iterator<char>());
	GLuint shader = glCreateShader(shaderType);
	auto ptr = (const GLchar*)source.c_str();
	
	glShaderSourcePrint(shader, 1, &ptr, nullptr);
	glCompileShader(shader);

	// print error log if failed to compile
	int success; glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		int length; glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
		string log(length + 1, '\0');
		glGetShaderInfoLog(shader, length + 1, &length, &log[0]);
		printf("log of compiling %s:\n%s\n", path.c_str(), log.c_str());
	}
	return shader;
}

// creates a compute program based on a single file
GLuint createProgram(const std::string& computePath) {
	using namespace std;
	GLuint program = glCreateProgram();
	GLuint computeShader = createShader(computePath, GL_COMPUTE_SHADER);
	glAttachShader(program, computeShader);
	glLinkProgram(program);
	glDeleteShader(computeShader);

	// print error log if failed to link
	int success; glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		int length; glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
		string log(length + 1, '\0');
		glGetProgramInfoLog(program, length + 1, &length, &log[0]);
		printf("log of linking %s:\n%s\n", computePath.c_str(), log.c_str());
		glDeleteProgram(program);
		return 0;
	}
	return program;
}

// creates a graphics program based on shaders for potentially all 5 programmable pipeline stages
GLuint createProgram(const std::string& vertexPath, const std::string& controlPath, const std::string& evaluationPath, const std::string& geometryPath, const std::string& fragmentPath) {
	using namespace std;
	GLuint program = glCreateProgram();
	std::string paths[] = { vertexPath, controlPath, evaluationPath, geometryPath, fragmentPath};
	GLenum types[] = {GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER};
	for (int i = 0; i < 5; ++i) {
		if (!paths[i].length()) continue;
		GLuint shader = createShader(paths[i], types[i]);
		glAttachShader(program, shader);
		glDeleteShader(shader); // deleting here is okay; the shader object is reference-counted with the programs it's attached to
	}
	glLinkProgram(program);

	// print error log if failed to link
	int success; glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		int length; glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
		string log(length + 1, '\0');
		glGetProgramInfoLog(program, length + 1, &length, &log[0]);
		printf("log of linking (%s,%s,%s,%s,%s):\n%s\n", vertexPath.c_str(), controlPath.c_str(), evaluationPath.c_str(), geometryPath.c_str(), fragmentPath.c_str(), log.c_str());
		glDeleteProgram(program);
		return 0;
	}
	return program;
}

// helper to avoid passing the current program around
GLuint currentProgram() {
	GLuint program;
	glGetIntegerv(GL_CURRENT_PROGRAM, (GLint*)&program);
	return program;
}

// generic buffer binder for block-type buffers (shader storage, uniform)
constexpr GLenum bindingProperty = GL_BUFFER_BINDING;
constexpr GLenum typeProperty = GL_TYPE;
void bindBuffer(const std::string& name, GLuint buffer) {
	GLuint program = currentProgram(), index = glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK, name.c_str());
	GLenum type;
	//what was this for? //glGetProgramResourceiv(program, GL_UNIFORM, index, 1, &typeProperty, sizeof(type), nullptr, (GLint*)&type);
	if (index != GL_INVALID_INDEX) {
		glShaderStorageBlockBinding(program, index, index);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, buffer);
	}
	else {
		index = glGetProgramResourceIndex(program, GL_UNIFORM_BLOCK, name.c_str());
		glUniformBlockBinding(program, index, index);
		glBindBufferBase(GL_UNIFORM_BUFFER, index, buffer);
	}
	//printf("buffer %s bound to slot %d!\n", name.c_str(), index);
}

const GLenum samplerTypes[] = { GL_SAMPLER_1D, GL_SAMPLER_2D, GL_SAMPLER_3D, GL_SAMPLER_CUBE, GL_SAMPLER_1D_SHADOW, GL_SAMPLER_2D_SHADOW, GL_SAMPLER_1D_ARRAY, GL_SAMPLER_2D_ARRAY, GL_SAMPLER_CUBE_MAP_ARRAY, GL_SAMPLER_1D_ARRAY_SHADOW,GL_SAMPLER_2D_ARRAY_SHADOW, GL_SAMPLER_2D_MULTISAMPLE,GL_SAMPLER_2D_MULTISAMPLE_ARRAY, GL_SAMPLER_CUBE_SHADOW, GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW, GL_SAMPLER_BUFFER, GL_SAMPLER_2D_RECT, GL_SAMPLER_2D_RECT_SHADOW, GL_INT_SAMPLER_1D, GL_INT_SAMPLER_2D, GL_INT_SAMPLER_3D, GL_INT_SAMPLER_CUBE, GL_INT_SAMPLER_1D_ARRAY, GL_INT_SAMPLER_2D_ARRAY, GL_INT_SAMPLER_CUBE_MAP_ARRAY, GL_INT_SAMPLER_2D_MULTISAMPLE, GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY, GL_INT_SAMPLER_BUFFER, GL_INT_SAMPLER_2D_RECT, GL_UNSIGNED_INT_SAMPLER_1D, GL_UNSIGNED_INT_SAMPLER_2D, GL_UNSIGNED_INT_SAMPLER_3D, GL_UNSIGNED_INT_SAMPLER_CUBE, GL_UNSIGNED_INT_SAMPLER_1D_ARRAY, GL_UNSIGNED_INT_SAMPLER_2D_ARRAY, GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY, GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE, GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY,GL_UNSIGNED_INT_SAMPLER_BUFFER, GL_UNSIGNED_INT_SAMPLER_2D_RECT };
const GLenum imageTypes[] = { GL_IMAGE_1D, GL_IMAGE_2D, GL_IMAGE_3D, GL_IMAGE_2D_RECT, GL_IMAGE_CUBE, GL_IMAGE_BUFFER, GL_IMAGE_1D_ARRAY, GL_IMAGE_2D_ARRAY, GL_IMAGE_CUBE_MAP_ARRAY, GL_IMAGE_2D_MULTISAMPLE, GL_IMAGE_2D_MULTISAMPLE_ARRAY, GL_INT_IMAGE_1D, GL_INT_IMAGE_2D, GL_INT_IMAGE_3D, GL_INT_IMAGE_2D_RECT, GL_INT_IMAGE_CUBE, GL_INT_IMAGE_BUFFER, GL_INT_IMAGE_1D_ARRAY, GL_INT_IMAGE_2D_ARRAY, GL_INT_IMAGE_CUBE_MAP_ARRAY, GL_INT_IMAGE_2D_MULTISAMPLE, GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY, GL_UNSIGNED_INT_IMAGE_1D, GL_UNSIGNED_INT_IMAGE_2D, GL_UNSIGNED_INT_IMAGE_3D, GL_UNSIGNED_INT_IMAGE_CUBE, GL_UNSIGNED_INT_IMAGE_BUFFER, GL_UNSIGNED_INT_IMAGE_1D_ARRAY, GL_UNSIGNED_INT_IMAGE_2D_ARRAY, GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY, GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE, GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY };

bool isOfType(const GLenum type, const GLenum* types, const GLint typeCount) {
	for (int i = 0; i < typeCount; ++i)
		if (type == types[i])
			return true;
	return false;
}

// counts the amount of objects of the same type before this one in the arbitrary order the API happens to give them; this gives a unique index for each object that's used for the texture and image unit
GLint findUnit(GLuint program, const std::string& name, const GLenum* types, const GLint typeCount) {
	GLint unit = 0;
	GLuint index = glGetProgramResourceIndex(program, GL_UNIFORM, name.c_str());
	if (index == GL_INVALID_INDEX) return -1;
	for (unsigned i = 0; i < index; ++i) {
		GLenum type;
		glGetProgramResourceiv(program, GL_UNIFORM, i, 1, &typeProperty, sizeof(type), nullptr, (GLint*)&type);
		unit += GLuint(isOfType(type, types, typeCount));
	}
	return unit;
}

void bindTexture(const std::string& name, GLenum target, GLuint texture) {
	GLuint program = currentProgram();
	GLint unit = findUnit(program, name, samplerTypes, sizeof(samplerTypes) / sizeof(GLenum));
	if (unit < 0) return;
	glActiveTexture(GL_TEXTURE0 + unit);
	glUniform1i(glGetUniformLocation(program, name.c_str()), unit);
	glBindTexture(target, texture);
	printf("texture %s bound to texture unit %d!\n", name.c_str(), unit);
}

void bindImage(const std::string& name, GLint level, GLuint texture, GLenum access, GLenum format) {
	GLuint program = currentProgram();
	GLint unit = findUnit(program, name, imageTypes, sizeof(imageTypes) / sizeof(GLenum));
	glUniform1i(glGetUniformLocation(program, name.c_str()), unit);
	glBindImageTexture(unit, texture, level, true, 0, access, format);
	printf("image %s bound to image unit %d!\n", name.c_str(), unit);
}

void bindImageLayer(const std::string& name, GLint level, GLint layer, GLuint texture, GLenum access, GLenum format) {
	GLuint program = currentProgram(), unit = findUnit(program, name, imageTypes, sizeof(imageTypes) / sizeof(GLenum));
	glUniform1ui(glGetUniformLocation(program, name.c_str()), unit);
	glBindImageTexture(unit, texture, level, false, layer, access, format);
	printf("layer %d of image %s bound to image unit %d!\n", layer, name.c_str(), unit);
}

// sets the drawbuffers based on bound attachments.
void setDrawBuffers() {
	for (int i = 0; i < bufferCount; ++i) {
		GLint type;
		glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
		drawBuffers[i] = (type != GL_NONE) ? (GL_COLOR_ATTACHMENT0 + i) : GL_NONE;
	}
	glDrawBuffers(bufferCount, drawBuffers);
}

// the bind point is chosen to also be the attachment for convenience.
void bindOutputTexture(const std::string& name, GLuint texture, GLint level) {
	GLint location = glGetProgramResourceLocation(currentProgram(), GL_PROGRAM_OUTPUT, name.c_str());
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + location, texture, level);
	setDrawBuffers();
	GLint width, height;
	glGetTextureLevelParameteriv(texture, level, GL_TEXTURE_WIDTH, &width);
	glGetTextureLevelParameteriv(texture, level, GL_TEXTURE_HEIGHT, &height);
	glViewport(0, 0, width, height);
	//printf("texture %s bound to fragment output %d!\n", name.c_str(), location);
}

void bindOutputRenderbuffer(const std::string& name, GLuint renderbuffer, GLint level) {
	GLint location = glGetProgramResourceLocation(currentProgram(), GL_PROGRAM_OUTPUT, name.c_str());
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + location, renderbuffer, level);
	setDrawBuffers();
	GLint width, height;
	glGetRenderbufferParameteriv(renderbuffer, GL_RENDERBUFFER_WIDTH, &width);
	glGetRenderbufferParameteriv(renderbuffer, GL_RENDERBUFFER_HEIGHT, &height);
	glViewport(0, 0, width, height);
	printf("renderbuffer %s bound to fragment output %d!\n", name.c_str(), location);
}

// all uniform functions using the name directly instead of the getuniformlocation trouble
void glUniform1i(const std::string& name, GLint value) { glUniform1i(glGetUniformLocation(currentProgram(), name.c_str()), value); }
void glUniform2i(const std::string& name, GLint value1, GLint value2) { glUniform2i(glGetUniformLocation(currentProgram(), name.c_str()), value1, value2); }
void glUniform3i(const std::string& name, GLint value1, GLint value2, GLint value3) { glUniform3i(glGetUniformLocation(currentProgram(), name.c_str()), value1, value2, value3); }
void glUniform4i(const std::string& name, GLint value1, GLint value2, GLint value3, GLint value4) { glUniform4i(glGetUniformLocation(currentProgram(), name.c_str()), value1, value2, value3, value4); }

void glUniform1ui(const std::string& name, GLuint value) { glUniform1ui(glGetUniformLocation(currentProgram(), name.c_str()), value); }
void glUniform2ui(const std::string& name, GLuint value1, GLuint value2) { glUniform2ui(glGetUniformLocation(currentProgram(), name.c_str()), value1, value2); }
void glUniform3ui(const std::string& name, GLuint value1, GLuint value2, GLuint value3) { glUniform3ui(glGetUniformLocation(currentProgram(), name.c_str()), value1, value2, value3); }
void glUniform4ui(const std::string& name, GLuint value1, GLuint value2, GLuint value3, GLuint value4) { glUniform4ui(glGetUniformLocation(currentProgram(), name.c_str()), value1, value2, value3, value4); }

void glUniform1f(const std::string& name, GLfloat value) { glUniform1f(glGetUniformLocation(currentProgram(), name.c_str()), value); }
void glUniform2f(const std::string& name, GLfloat value1, GLfloat value2) { glUniform2f(glGetUniformLocation(currentProgram(), name.c_str()), value1, value2); }
void glUniform3f(const std::string& name, GLfloat value1, GLfloat value2, GLfloat value3) { glUniform3f(glGetUniformLocation(currentProgram(), name.c_str()), value1, value2, value3); }
void glUniform4f(const std::string& name, GLfloat value1, GLfloat value2, GLfloat value3, GLfloat value4) { glUniform4f(glGetUniformLocation(currentProgram(), name.c_str()), value1, value2, value3, value4); }

#define uniform_v(func, type) void glU##func(const std::string& name, GLsizei count, const type* value) { glU##func(glGetUniformLocation(currentProgram(), name.c_str()), count, value); }
uniform_v(niform1iv, GLint)
uniform_v(niform2iv, GLint)
uniform_v(niform3iv, GLint)
uniform_v(niform4iv, GLint)

uniform_v(niform1uiv, GLuint)
uniform_v(niform2uiv, GLuint)
uniform_v(niform3uiv, GLuint)
uniform_v(niform4uiv, GLuint)

uniform_v(niform1fv, GLfloat)
uniform_v(niform2fv, GLfloat)
uniform_v(niform3fv, GLfloat)
uniform_v(niform4fv, GLfloat)
#undef uniform_v

#define uniform_matrix(func) void glU##func(const std::string& name, GLsizei count, GLboolean transpose, const GLfloat *value) {glU##func(glGetUniformLocation(currentProgram(), name.c_str()), count, transpose, value);}
uniform_matrix(niformMatrix2fv)
uniform_matrix(niformMatrix3fv)
uniform_matrix(niformMatrix4fv)
uniform_matrix(niformMatrix2x3fv)
uniform_matrix(niformMatrix3x2fv)
uniform_matrix(niformMatrix2x4fv)
uniform_matrix(niformMatrix4x2fv)
uniform_matrix(niformMatrix3x4fv)
uniform_matrix(niformMatrix4x3fv)
#undef uniform_matrix

void lookAt(float* cameraToWorld, float camx, float camy, float camz, float atx, float aty, float atz, float upx, float upy, float upz) {
	atx -= camx; aty -= camy; atz -= camz;
	float atrl = 1.f / sqrt(atx * atx + aty * aty + atz * atz); atx *= atrl; aty *= atrl; atz *= atrl;
	float uprl = 1.f / sqrt(upx * upx + upy * upy + upz * upz); upx *= uprl; upy *= uprl; upz *= uprl;
	float rightx = aty * upz - atz * upy, righty = atz * upx - atx * upz, rightz = atx * upy - aty * upx;
	float rrl = 1.f / sqrt(rightx*rightx + righty * righty + rightz * rightz); rightx *= rrl; righty *= rrl; rightz *= rrl;
	upx = righty * atz - rightz * aty; upy = rightz * atx - rightx * atz; upz = rightx * aty - righty * atx;
	cameraToWorld[0] = rightx; cameraToWorld[1] = righty; cameraToWorld[2] = rightz; cameraToWorld[3] = .0f;
	cameraToWorld[4] = upx; cameraToWorld[5] = upy; cameraToWorld[6] = upz; cameraToWorld[7] = .0f;
	cameraToWorld[8] = -atx; cameraToWorld[9] = -aty; cameraToWorld[10] = -atz; cameraToWorld[11] = .0f;
	cameraToWorld[12] = camx; cameraToWorld[13] = camy; cameraToWorld[14] = camz; cameraToWorld[15] = 1.f;
}

void setupProjection(float* projection, float fov, float aspect, float nearPlane, float farPlane) {
	for (int i = 0; i < 16; ++i) projection[i] = .0f;
	const float right = nearPlane * aspect * atan(fov / 2.0f), bottom = nearPlane * atan(fov / 2.0f);
	projection[0] = nearPlane / right;
	projection[5] = nearPlane / bottom;
	projection[10] = -(farPlane + nearPlane) / (farPlane - nearPlane);
	projection[11] = -1.0f;
	projection[14] = -2.0f * farPlane * nearPlane / (farPlane - nearPlane);
}

void setupOrtho(float* ortho, float w_over_h, float size, float nearPlane, float farPlane) {
	for (int i = 0; i < 16; ++i) ortho[i] = .0f;
	ortho[0] = 1.f / size / w_over_h;
	ortho[5] = 1.f / size;
	ortho[10] = -2.f / (farPlane - nearPlane);
	ortho[14] = -1.f - 2.f*nearPlane / (farPlane - nearPlane);
	ortho[15] = 1.f;
}


#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#include <locale>
#include <codecvt>

Texture<GL_TEXTURE_2D> loadImage(const std::string& path) {
	using namespace Gdiplus;
	ULONG_PTR token;
	GdiplusStartup(&token, &GdiplusStartupInput(), nullptr);

	Texture<GL_TEXTURE_2D> result;
	{
		Image image(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(path).c_str());
		Rect r(0, 0, image.GetWidth(), image.GetHeight());
		BitmapData data;
		((Bitmap*)&image)->LockBits(&r, ImageLockModeRead, PixelFormat24bppRGB, &data);

		glBindTexture(GL_TEXTURE_2D, result);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, image.GetWidth(), image.GetHeight(), 0, GL_BGR, GL_UNSIGNED_BYTE, data.Scan0);
		glGenerateMipmap(GL_TEXTURE_2D);

		((Bitmap*)&image)->UnlockBits(&data);
	}
	GdiplusShutdown(token);
	return result;
}
