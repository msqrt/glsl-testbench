
#include "gl_helpers.h"
#include "shaderprintf.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <array>
#include <map>
#include <filesystem>

// whenever a shader is created, we go through all of its uniforms and assign unit indices for textures and images.
const GLenum samplerTypes[] = { GL_SAMPLER_1D, GL_SAMPLER_2D, GL_SAMPLER_3D, GL_SAMPLER_CUBE, GL_SAMPLER_1D_SHADOW, GL_SAMPLER_2D_SHADOW, GL_SAMPLER_1D_ARRAY, GL_SAMPLER_2D_ARRAY, GL_SAMPLER_CUBE_MAP_ARRAY, GL_SAMPLER_1D_ARRAY_SHADOW,GL_SAMPLER_2D_ARRAY_SHADOW, GL_SAMPLER_2D_MULTISAMPLE,GL_SAMPLER_2D_MULTISAMPLE_ARRAY, GL_SAMPLER_CUBE_SHADOW, GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW, GL_SAMPLER_BUFFER, GL_SAMPLER_2D_RECT, GL_SAMPLER_2D_RECT_SHADOW, GL_INT_SAMPLER_1D, GL_INT_SAMPLER_2D, GL_INT_SAMPLER_3D, GL_INT_SAMPLER_CUBE, GL_INT_SAMPLER_1D_ARRAY, GL_INT_SAMPLER_2D_ARRAY, GL_INT_SAMPLER_CUBE_MAP_ARRAY, GL_INT_SAMPLER_2D_MULTISAMPLE, GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY, GL_INT_SAMPLER_BUFFER, GL_INT_SAMPLER_2D_RECT, GL_UNSIGNED_INT_SAMPLER_1D, GL_UNSIGNED_INT_SAMPLER_2D, GL_UNSIGNED_INT_SAMPLER_3D, GL_UNSIGNED_INT_SAMPLER_CUBE, GL_UNSIGNED_INT_SAMPLER_1D_ARRAY, GL_UNSIGNED_INT_SAMPLER_2D_ARRAY, GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY, GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE, GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY,GL_UNSIGNED_INT_SAMPLER_BUFFER, GL_UNSIGNED_INT_SAMPLER_2D_RECT };
const GLenum imageTypes[] = { GL_IMAGE_1D, GL_IMAGE_2D, GL_IMAGE_3D, GL_IMAGE_2D_RECT, GL_IMAGE_CUBE, GL_IMAGE_BUFFER, GL_IMAGE_1D_ARRAY, GL_IMAGE_2D_ARRAY, GL_IMAGE_CUBE_MAP_ARRAY, GL_IMAGE_2D_MULTISAMPLE, GL_IMAGE_2D_MULTISAMPLE_ARRAY, GL_INT_IMAGE_1D, GL_INT_IMAGE_2D, GL_INT_IMAGE_3D, GL_INT_IMAGE_2D_RECT, GL_INT_IMAGE_CUBE, GL_INT_IMAGE_BUFFER, GL_INT_IMAGE_1D_ARRAY, GL_INT_IMAGE_2D_ARRAY, GL_INT_IMAGE_CUBE_MAP_ARRAY, GL_INT_IMAGE_2D_MULTISAMPLE, GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY, GL_UNSIGNED_INT_IMAGE_1D, GL_UNSIGNED_INT_IMAGE_2D, GL_UNSIGNED_INT_IMAGE_3D, GL_UNSIGNED_INT_IMAGE_CUBE, GL_UNSIGNED_INT_IMAGE_BUFFER, GL_UNSIGNED_INT_IMAGE_1D_ARRAY, GL_UNSIGNED_INT_IMAGE_2D_ARRAY, GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY, GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE, GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY };

bool isOfType(const GLenum type, const GLenum* types, const GLint typeCount) {
	for (int i = 0; i < typeCount; ++i)
		if (type == types[i])
			return true;
	return false;
}

struct shaderReloadState {
	int count = 0;
	std::filesystem::file_time_type lastLoad;
	std::array<std::string, 5> paths;

	void addPath(const std::string& path) {
		if (path.length() > 0)
			for (int i = 0; i < count + 1; ++i)
				if (i == count) {
					paths[i] = path;
					std::error_code ec;
					auto fileModify = std::filesystem::last_write_time(std::filesystem::path(path), ec);
					if (!ec && fileModify > lastLoad)
						lastLoad = fileModify;
					count++;
					return;
				}
				else if (paths[i] == path)
					break;
	}
};

std::map<GLuint, shaderReloadState> shaderMap;

void pruneShaderReloadMap() {
	for (auto i = shaderMap.begin(); i != shaderMap.end(); ++i)
		if (!glIsProgram(i->first))
			i = shaderMap.erase(i);
}

Program createProgram(const std::string& computePath) {
	return Program(computePath);
}
Program createProgram(const std::string& vertexPath, const std::string& controlPath, const std::string& evaluationPath, const std::string& geometryPath, const std::string& fragmentPath) {
	return Program(vertexPath, controlPath, evaluationPath, geometryPath, fragmentPath);
}

bool reloadRequired(GLuint program) {
	if (program > 0) {
		if (auto i = shaderMap.find(program); i != shaderMap.end()) {
			auto& state = i->second;
			for (int j = 0; j < state.count; ++j) {
				std::error_code ec;
				auto fileModify = std::filesystem::last_write_time(std::filesystem::path(state.paths[j]), ec);
				if (!ec && fileModify > state.lastLoad) {
					state.lastLoad = fileModify;
					return true;
				}
			}
		}
		return false;
	}
	return true;
}

const GLenum typeProperty = GL_TYPE;
const GLenum locationProperty = GL_LOCATION;
// counts the amount of objects of the same type before this one in the arbitrary order the API happens to give them; this gives a unique index for each object that's used for the texture and image unit



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
	}
	else {
		index = glGetProgramResourceIndex(program, GL_UNIFORM_BLOCK, name.c_str());
		glUniformBlockBinding(program, index, index);
		glBindBufferBase(GL_UNIFORM_BUFFER, index, buffer);
		//printf("uniform buffer %s bound to slot %d!\n", name.c_str(), index);
	}
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
	GdiplusStartup(&token, &GdiplusStartupInput(), nullptr);

	Texture<GL_TEXTURE_2D> result;
	// new scope so RAII objects are released before gdi+ shutdown
	{
		Image image(path.c_str());
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
