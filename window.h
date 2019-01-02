
#pragma once

#include <Windows.h>

#include <string>
#include <fstream>

#include <gl/gl.h>
#include "loadgl/wglext.h"
#include "loadgl/glext.h"
#include "loadgl/loadgl46.h"

void setupGL(int width, int height, const std::string& title, bool fullscreen);
void closeGL();
void swapBuffers();
void setTitle(const std::string& title);

struct OpenGL {
	OpenGL(int width, int height, const std::string& title, bool fullscreen) {
		setupGL(width, height, title, fullscreen);
	}
	~OpenGL() {
		closeGL();
	}
};

POINT getMouse();
void setMouse(POINT);

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
	GLuint object;
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
	GLuint object;
	inline Buffer() { glCreateBuffers(1, &object); }
	inline Buffer(const GLuint & other) { object = other; }
	inline Buffer& operator=(Buffer & other) {
		glDeleteBuffers(1, &object);
		std::swap(object, other.object);
		return *this;
	}
	inline void swap(Buffer& other) { std::swap(object, other.object); }
	inline Buffer(Buffer && other) { *this = other; }
	inline operator GLuint() { return object; }
	inline ~Buffer() { glDeleteBuffers(1, &object); }
};

template <GLenum target>
struct Texture {
	GLuint object;
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

struct TimeStamp {
	GLuint synchronousObject, available = GL_FALSE;
	GLint64 synchronous, asynchronous;
	TimeStamp() {
		glCreateQueries(GL_TIMESTAMP, 1, &synchronousObject);
		glGetInteger64v(GL_TIMESTAMP, &asynchronous);
		glQueryCounter(synchronousObject, GL_TIMESTAMP);
	}
	inline void check() {
		if (available == GL_TRUE)
			return;
		while (available == GL_FALSE)
			glGetQueryObjectuiv(synchronousObject, GL_QUERY_RESULT_AVAILABLE, &available);
		glGetQueryObjecti64v(synchronousObject, GL_QUERY_RESULT, &synchronous);
	}
	~TimeStamp() {
		glDeleteQueries(1, &synchronousObject);
	}
};

// ms
inline double latency(TimeStamp& stamp) {
	stamp.check();
	return double(stamp.synchronous - stamp.asynchronous)*1.0e-6;
}

// ms
inline double elapsedTime(TimeStamp& begin, TimeStamp& end) {
	end.check();
	if (begin.available != GL_TRUE) {
		begin.available = GL_TRUE;
		glGetQueryObjecti64v(begin.synchronousObject, GL_QUERY_RESULT, &begin.synchronous);
	}
	return double(end.synchronous - begin.synchronous)*1.0e-6;
}

void lookAt(float* cameraToWorld, float camx, float camy, float camz, float atx, float aty, float atz, float upx = .0f, float upy = 1.f, float upz = .0f);
void setupProjection(float* projection, float fov, float aspect, float nearPlane, float farPlane);
void setupOrtho(float* ortho, float w_over_h, float size, float nearPlane = .1f, float farPlane = 20.f);
