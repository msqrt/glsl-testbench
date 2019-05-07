
#include "gl_helpers.h"
#include "shaderprintf.h"

#include <fstream>
#include <sstream>

// whenever a shader is created, we go through all of its uniforms and assign unit indices for textures and images.

const GLenum samplerTypes[] = { GL_SAMPLER_1D, GL_SAMPLER_2D, GL_SAMPLER_3D, GL_SAMPLER_CUBE, GL_SAMPLER_1D_SHADOW, GL_SAMPLER_2D_SHADOW, GL_SAMPLER_1D_ARRAY, GL_SAMPLER_2D_ARRAY, GL_SAMPLER_CUBE_MAP_ARRAY, GL_SAMPLER_1D_ARRAY_SHADOW,GL_SAMPLER_2D_ARRAY_SHADOW, GL_SAMPLER_2D_MULTISAMPLE,GL_SAMPLER_2D_MULTISAMPLE_ARRAY, GL_SAMPLER_CUBE_SHADOW, GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW, GL_SAMPLER_BUFFER, GL_SAMPLER_2D_RECT, GL_SAMPLER_2D_RECT_SHADOW, GL_INT_SAMPLER_1D, GL_INT_SAMPLER_2D, GL_INT_SAMPLER_3D, GL_INT_SAMPLER_CUBE, GL_INT_SAMPLER_1D_ARRAY, GL_INT_SAMPLER_2D_ARRAY, GL_INT_SAMPLER_CUBE_MAP_ARRAY, GL_INT_SAMPLER_2D_MULTISAMPLE, GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY, GL_INT_SAMPLER_BUFFER, GL_INT_SAMPLER_2D_RECT, GL_UNSIGNED_INT_SAMPLER_1D, GL_UNSIGNED_INT_SAMPLER_2D, GL_UNSIGNED_INT_SAMPLER_3D, GL_UNSIGNED_INT_SAMPLER_CUBE, GL_UNSIGNED_INT_SAMPLER_1D_ARRAY, GL_UNSIGNED_INT_SAMPLER_2D_ARRAY, GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY, GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE, GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY,GL_UNSIGNED_INT_SAMPLER_BUFFER, GL_UNSIGNED_INT_SAMPLER_2D_RECT };
const GLenum imageTypes[] = { GL_IMAGE_1D, GL_IMAGE_2D, GL_IMAGE_3D, GL_IMAGE_2D_RECT, GL_IMAGE_CUBE, GL_IMAGE_BUFFER, GL_IMAGE_1D_ARRAY, GL_IMAGE_2D_ARRAY, GL_IMAGE_CUBE_MAP_ARRAY, GL_IMAGE_2D_MULTISAMPLE, GL_IMAGE_2D_MULTISAMPLE_ARRAY, GL_INT_IMAGE_1D, GL_INT_IMAGE_2D, GL_INT_IMAGE_3D, GL_INT_IMAGE_2D_RECT, GL_INT_IMAGE_CUBE, GL_INT_IMAGE_BUFFER, GL_INT_IMAGE_1D_ARRAY, GL_INT_IMAGE_2D_ARRAY, GL_INT_IMAGE_CUBE_MAP_ARRAY, GL_INT_IMAGE_2D_MULTISAMPLE, GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY, GL_UNSIGNED_INT_IMAGE_1D, GL_UNSIGNED_INT_IMAGE_2D, GL_UNSIGNED_INT_IMAGE_3D, GL_UNSIGNED_INT_IMAGE_CUBE, GL_UNSIGNED_INT_IMAGE_BUFFER, GL_UNSIGNED_INT_IMAGE_1D_ARRAY, GL_UNSIGNED_INT_IMAGE_2D_ARRAY, GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY, GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE, GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY };

bool isOfType(const GLenum type, const GLenum* types, const GLint typeCount) {
	for (int i = 0; i < typeCount; ++i)
		if (type == types[i])
			return true;
	return false;
}

constexpr GLenum typeProperty = GL_TYPE;
constexpr GLenum locationProperty = GL_LOCATION;
// counts the amount of objects of the same type before this one in the arbitrary order the API happens to give them; this gives a unique index for each object that's used for the texture and image unit
void assignUnits(GLuint program, const GLenum* types, const GLint typeCount) {
	GLint unit = 0, location, type, count;
	glGetProgramInterfaceiv(program, GL_UNIFORM, GL_ACTIVE_RESOURCES, &count);
	for (unsigned i = 0; i < count; ++i) {
		glGetProgramResourceiv(program, GL_UNIFORM, i, 1, &typeProperty, sizeof(type), nullptr, &type);
		if (isOfType((GLenum)type, types, typeCount)) {
			glGetProgramResourceiv(program, GL_UNIFORM, i, 1, &locationProperty, sizeof(location), nullptr, &location);
			glUniform1i(location, unit);
			unit++;
		}
	}
}

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

	glUseProgram(program);
	assignUnits(program, samplerTypes, sizeof(samplerTypes) / sizeof(GLenum));
	assignUnits(program, imageTypes, sizeof(imageTypes) / sizeof(GLenum));

	return program;
}

// creates a graphics program based on shaders for potentially all 5 programmable pipeline stages
GLuint createProgram(const std::string& vertexPath, const std::string& controlPath, const std::string& evaluationPath, const std::string& geometryPath, const std::string& fragmentPath) {
	using namespace std;
	GLuint program = glCreateProgram();
	std::string paths[] = { vertexPath, controlPath, evaluationPath, geometryPath, fragmentPath };
	GLenum types[] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER };
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

	glUseProgram(program);
	assignUnits(program, samplerTypes, sizeof(samplerTypes) / sizeof(GLenum));
	assignUnits(program, imageTypes, sizeof(imageTypes) / sizeof(GLenum));

	return program;
}

// helper to avoid passing the current program around
GLuint currentProgram() {
	GLuint program;
	glGetIntegerv(GL_CURRENT_PROGRAM, (GLint*)&program);
	return program;
}

// generic buffer binder for block-type buffers (shader storage, uniform)
void bindBuffer(const std::string& name, GLuint buffer) {
	GLuint program = currentProgram(), index = glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK, name.c_str());
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

void bindTexture(const std::string& name, GLenum target, GLuint texture) {
	GLuint program = currentProgram();
	GLint unit, location = glGetUniformLocation(program, name.c_str());
	if (location < 0) return;
	glGetUniformiv(program, location, &unit);
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(target, texture);
	//printf("texture %s bound to texture unit %d!\n", name.c_str(), unit);
}

void bindImage(const std::string& name, GLint level, GLuint texture, GLenum access, GLenum format) {
	GLuint program = currentProgram();
	GLint unit, location = glGetUniformLocation(program, name.c_str());
	if (location < 0) return;
	glGetUniformiv(program, location, &unit);
	glBindImageTexture(unit, texture, level, true, 0, access, format);
	//printf("image %s bound to image unit %d!\n", name.c_str(), unit);
}

void bindImageLayer(const std::string& name, GLint level, GLint layer, GLuint texture, GLenum access, GLenum format) {
	GLuint program = currentProgram();
	GLint unit, location = glGetUniformLocation(program, name.c_str());
	if (location < 0) return;
	glGetUniformiv(program, location, &unit);
	glBindImageTexture(unit, texture, level, false, layer, access, format);
	//printf("layer %d of image %s bound to image unit %d!\n", layer, name.c_str(), unit);
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
	glDrawBuffers(drawBuffers.size(), drawBuffers.data());
}

// the bind point is chosen to also be the attachment for convenience.
void bindOutputTexture(const std::string& name, GLuint texture, GLint level) {
	GLint location = glGetProgramResourceLocation(currentProgram(), GL_PROGRAM_OUTPUT, name.c_str());
	if (location < 0) return;
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
	//printf("renderbuffer %s bound to fragment output %d!\n", name.c_str(), location);
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
