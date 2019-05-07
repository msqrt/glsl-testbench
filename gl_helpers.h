#pragma once

#include <Windows.h>

#include <string>

#include <gl/gl.h>
#include "loadgl/glext.h"
#include "loadgl/loadgl46.h"

GLuint createProgram(const std::string& computePath);
GLuint createProgram(const std::string& vertexPath, const std::string& controlPath, const std::string& evaluationPath, const std::string& geometryPath, const std::string& fragmentPath);

void bindBuffer(const std::string& name, GLuint buffer);

void bindTexture(const std::string& name, GLenum target, GLuint texture);
void bindImage(const std::string& name, GLint level, GLuint texture, GLenum access, GLenum format);
void bindImageLayer(const std::string& name, GLint level, GLint layer, GLuint texture, GLenum access, GLenum format);

void bindOutputTexture(const std::string& name, GLuint texture, GLint level);
void bindOutputRenderbuffer(const std::string& name, GLuint renderbuffer, GLint level);

void glUniform1i(const std::string& name, GLint value);
void glUniform2i(const std::string& name, GLint value1, GLint value2);
void glUniform3i(const std::string& name, GLint value1, GLint value2, GLint value3);
void glUniform4i(const std::string& name, GLint value1, GLint value2, GLint value3, GLint value4);

void glUniform1ui(const std::string& name, GLuint value);
void glUniform2ui(const std::string& name, GLuint value1, GLuint value2);
void glUniform3ui(const std::string& name, GLuint value1, GLuint value2, GLuint value3);
void glUniform4ui(const std::string& name, GLuint value1, GLuint value2, GLuint value3, GLuint value4);

void glUniform1f(const std::string& name, GLfloat value);
void glUniform2f(const std::string& name, GLfloat value1, GLfloat value2);
void glUniform3f(const std::string& name, GLfloat value1, GLfloat value2, GLfloat value3);
void glUniform4f(const std::string& name, GLfloat value1, GLfloat value2, GLfloat value3, GLfloat value4);

#define uniform_v(func, type) void glU##func(const std::string& name, GLsizei count, const type* value);
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

#define uniform_matrix(func) void glU##func(const std::string& name, GLsizei count, GLboolean transpose, const GLfloat *value);
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

struct Program {
	GLuint object = 0;
	inline Program() {}
	inline Program(const GLuint & other) { object = other; } // hurr
	inline Program& operator=(const GLuint & other) { object = other; return *this; } // durr, thanks language for requiring copying code
	inline Program& operator=(Program& other) {
		std::swap(object, other.object);
		return *this;
	}
	inline operator GLuint() { return object; }
	inline operator bool() { return object != 0; }
	inline ~Program() { glDeleteProgram(object); }
};

struct Framebuffer {
	GLuint object = 0;
	inline Framebuffer() { glCreateFramebuffers(1, &object); }
	inline Framebuffer(const GLuint & other) { object = other; }
	inline Framebuffer& operator=(Framebuffer & other) {
		glDeleteFramebuffers(1, &object);
		std::swap(object, other.object);
		return *this;
	}
	inline Framebuffer(Framebuffer && other) { *this = other; }
	inline operator GLuint() { return object; }
	inline ~Framebuffer() { glDeleteFramebuffers(1, &object); }
};

struct Buffer {
	GLuint object = 0;
	inline Buffer() { glCreateBuffers(1, &object); }
	inline Buffer(const GLuint & other) { object = other; }
	inline Buffer& operator=(Buffer & other) {
		glDeleteBuffers(1, &object);
		std::swap(object, other.object);
		return *this;
	}
	inline Buffer(Buffer && other) { *this = other; }
	inline operator GLuint() { return object; }
	inline ~Buffer() { glDeleteBuffers(1, &object); }
};

template <GLenum target>
struct Texture {
	GLuint object = 0;
	inline Texture() { glCreateTextures(target, 1, &object); }
	inline Texture(const GLuint & other) { object = other; }
	inline Texture& operator=(Texture & other) {
		glDeleteTextures(1, &object);
		std::swap(object, other.object);
		return *this;
	}
	inline Texture(Texture && other) { *this = other; }
	inline operator GLuint() { return object; }
	inline ~Texture() { glDeleteTextures(1, &object); }
};
Texture<GL_TEXTURE_2D> loadImage(const std::string& path);
template<GLenum target> void bindTexture(const std::string& name, Texture<target>& texture) { bindTexture(name, target, texture); }
