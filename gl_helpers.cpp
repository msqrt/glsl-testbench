
#include "gl_helpers.h"
#include "shaderprintf.h"

Program createProgram(const std::string_view computePath) {
	return Program(computePath);
}
Program createProgram(const std::string_view vertexPath, const std::string_view controlPath, const std::string_view evaluationPath, const std::string_view geometryPath, const std::string_view fragmentPath) {
	return Program(vertexPath, controlPath, evaluationPath, geometryPath, fragmentPath);
}

// helper to avoid passing the current program around
GLuint currentProgram() {
	GLuint program;
	glGetIntegerv(GL_CURRENT_PROGRAM, (GLint*)&program);
	return program;
}

// generic buffer binder for block-type buffers (shader storage, uniform)
// TODO: is this necessary? how often do you change between these?
// would it be confusing to have bindBuffer and bindUniformBuffer? should we have bindStorageBuffer?
void bindBuffer(const std::string& name, GLuint buffer) {
	const GLuint program = currentProgram();
	GLuint index = glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK, name.c_str());
	if (index != GL_INVALID_INDEX) {
		glShaderStorageBlockBinding(program, index, index);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, buffer);
		//printf("storage buffer %s bound to slot %d!\n", name.c_str(), index);
		return;
	}

	index = glGetProgramResourceIndex(program, GL_UNIFORM_BLOCK, name.c_str());

	if (index != GL_INVALID_INDEX) {
		glUniformBlockBinding(program, index, index);
		glBindBufferBase(GL_UNIFORM_BUFFER, index, buffer);
		//printf("uniform buffer %s bound to slot %d!\n", name.c_str(), index);
		return;
	}

	// apparently this buffer was unused in the shader and optimized away
}

void bindTexture(const std::string& name, GLuint texture) {
	const GLuint program = currentProgram();
	const GLint location = glGetUniformLocation(program, name.c_str());
	if (location < 0) return; // todo: how to handle? unused samplers just drop from the shader
	GLint unit;
	glGetUniformiv(program, location, &unit);
	glBindTextureUnit(unit, texture);
	//printf("texture %s bound to texture unit %d!\n", name.c_str(), unit);
}

void bindImage(const std::string& name, GLint level, GLuint texture, GLenum access, GLenum format) {
	const GLuint program = currentProgram();
	const GLint location = glGetUniformLocation(program, name.c_str());
	if (location < 0) return;
	GLint unit;
	glGetUniformiv(program, location, &unit);
	glBindImageTexture(unit, texture, level, true, 0, access, format);
	//printf("image %s bound to image unit %d!\n", name.c_str(), unit);
}

void bindImageLayer(const std::string& name, GLint level, GLint layer, GLuint texture, GLenum access, GLenum format) {
	const GLuint program = currentProgram();
	const GLint location = glGetUniformLocation(program, name.c_str());
	if (location < 0) return;
	GLint unit;
	glGetUniformiv(program, location, &unit);
	glBindImageTexture(unit, texture, level, false, layer, access, format);
	//printf("layer %d of image %s bound to image unit %d!\n", layer, name.c_str(), unit);
}

void viewportFromTexture(GLuint texture, GLint level) {
	GLint width, height;
	glGetTextureLevelParameteriv(texture, level, GL_TEXTURE_WIDTH, &width);
	glGetTextureLevelParameteriv(texture, level, GL_TEXTURE_HEIGHT, &height);
	glViewport(0, 0, width, height);
}

void viewportFromRenderbuffer(GLuint renderbuffer) {
	GLint width, height;
	glGetRenderbufferParameteriv(renderbuffer, GL_RENDERBUFFER_WIDTH, &width);
	glGetRenderbufferParameteriv(renderbuffer, GL_RENDERBUFFER_HEIGHT, &height);
	glViewport(0, 0, width, height);
}

// sets the drawbuffers based on bound attachments.
std::vector<GLenum> drawBuffers;
void setDrawBuffers() {
	if (drawBuffers.size() == 0) {
		int bufferCount;
		glGetIntegerv(GL_MAX_DRAW_BUFFERS, &bufferCount);
		drawBuffers.resize(bufferCount);
	}
	for (int i = 0; i < drawBuffers.size(); ++i) {
		GLint type;
		glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
		drawBuffers[i] = (type != GL_NONE) ? (GL_COLOR_ATTACHMENT0 + i) : GL_NONE;
	}
	glDrawBuffers((GLsizei)drawBuffers.size(), drawBuffers.data());
}

// the bind point is chosen to also be the attachment for convenience.
void bindOutputTexture(const std::string& name, GLuint texture, GLint level) {
	const GLint location = glGetProgramResourceLocation(currentProgram(), GL_PROGRAM_OUTPUT, name.c_str());
	if (location < 0) return;
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + location, texture, level);
	setDrawBuffers();
	viewportFromTexture(texture, level);
	//printf("texture %s bound to fragment output %d!\n", name.c_str(), location);
}

void bindOutputRenderbuffer(const std::string& name, GLuint renderbuffer) {
	GLint location = glGetProgramResourceLocation(currentProgram(), GL_PROGRAM_OUTPUT, name.c_str());
	if (location < 0) return;
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + location, GL_RENDERBUFFER, renderbuffer);
	setDrawBuffers();
	viewportFromRenderbuffer(renderbuffer);
	//printf("renderbuffer %s bound to fragment output %d!\n", name.c_str(), location);
}

void bindOutputDepthTexture(GLuint texture, GLint level) {
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture, level);
	setDrawBuffers();
	viewportFromTexture(texture, level);
	//printf("texture %s bound to fragment output %d!\n", name.c_str(), location);
}

void bindOutputDepthRenderbuffer(GLuint renderbuffer) {
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);
	setDrawBuffers();
	viewportFromRenderbuffer(renderbuffer);
	//printf("renderbuffer %s bound to fragment output %d!\n", name.c_str(), location);
}

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#include <locale>
#include <codecvt>

Texture<GL_TEXTURE_2D> loadImage(const std::wstring& path) {
	using namespace Gdiplus;
	ULONG_PTR token;
	GdiplusStartupInput startupInput;
	GdiplusStartup(&token, &startupInput, nullptr);

	Texture<GL_TEXTURE_2D> result;
	// new scope so RAII objects are released before gdi+ shutdown
	{
		Image image(path.c_str());
		image.RotateFlip(RotateNoneFlipY);
		const Rect r(0, 0, image.GetWidth(), image.GetHeight());
		BitmapData data;
		((Bitmap*)&image)->LockBits(&r, ImageLockModeRead, PixelFormat24bppRGB, &data);

		glBindTexture(GL_TEXTURE_2D, result);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, image.GetWidth(), image.GetHeight(), 0, GL_BGR, GL_UNSIGNED_BYTE, data.Scan0);
		glGenerateMipmap(GL_TEXTURE_2D);

		((Bitmap*)&image)->UnlockBits(&data);
	}
	GdiplusShutdown(token);
	return result;
}

Texture<GL_TEXTURE_2D> loadImage(const std::string& path) {
	wchar_t str[4096];
	MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, path.c_str(), -1, str, 4096);
	return loadImage(std::wstring(str));
}

// all uniform functions using the name directly instead of the getuniformlocation trouble
#define uniform_copy(postfix, postfix_v, type)\
void glUniform1##postfix(const std::string& name, type value) { glUniform1##postfix(glGetUniformLocation(currentProgram(), name.c_str()), value); }\
void glUniform2##postfix(const std::string& name, type value1, type value2) { glUniform2##postfix(glGetUniformLocation(currentProgram(), name.c_str()), value1, value2); }\
void glUniform3##postfix(const std::string& name, type value1, type value2, type value3) { glUniform3##postfix(glGetUniformLocation(currentProgram(), name.c_str()), value1, value2, value3); }\
void glUniform4##postfix(const std::string& name, type value1, type value2, type value3, type value4) { glUniform4##postfix(glGetUniformLocation(currentProgram(), name.c_str()), value1, value2, value3, value4); }\
void glUniform1##postfix_v(const std::string& name, GLsizei count, const type* value) { glUniform1##postfix_v(glGetUniformLocation(currentProgram(), name.c_str()), count, value); }\
void glUniform2##postfix_v(const std::string& name, GLsizei count, const type* value) { glUniform2##postfix_v(glGetUniformLocation(currentProgram(), name.c_str()), count, value); }\
void glUniform3##postfix_v(const std::string& name, GLsizei count, const type* value) { glUniform3##postfix_v(glGetUniformLocation(currentProgram(), name.c_str()), count, value); }\
void glUniform4##postfix_v(const std::string& name, GLsizei count, const type* value) { glUniform4##postfix_v(glGetUniformLocation(currentProgram(), name.c_str()), count, value); }

uniform_copy(i, iv, GLint)
uniform_copy(ui, uiv, GLuint)
uniform_copy(f, fv, GLfloat)
#undef uniform_copy

#define uniform_matrix(func) void glUniformMatrix##func(const std::string& name, GLsizei count, GLboolean transpose, const GLfloat *value) {glUniformMatrix##func(glGetUniformLocation(currentProgram(), name.c_str()), count, transpose, value);}
uniform_matrix(2fv)
uniform_matrix(3fv)
uniform_matrix(4fv)
uniform_matrix(2x3fv)
uniform_matrix(3x2fv)
uniform_matrix(2x4fv)
uniform_matrix(4x2fv)
uniform_matrix(3x4fv)
uniform_matrix(4x3fv)
#undef uniform_matrix
