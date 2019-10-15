#pragma once

#include <Windows.h>

#include <string>

#include <gl/gl.h>
#include "loadgl/glext.h"
#include "loadgl/loadgl46.h"

GLuint createProgram(const std::string& computePath);
GLuint createProgram(const std::string& vertexPath, const std::string& controlPath, const std::string& evaluationPath, const std::string& geometryPath, const std::string& fragmentPath);

bool reloadRequired(GLuint program);
template<typename T>
inline void set_if_ok(T& old, const GLuint&& replacement) { if (replacement) old = replacement; }

void bindBuffer(const std::string& name, GLuint buffer);
void bindTexture(const std::string& name, GLuint texture);
// todo: can perhaps get the access, format by reflection?
void bindImage(const std::string& name, GLint level, GLuint texture, GLenum access, GLenum format);
void bindImageLayer(const std::string& name, GLint level, GLint layer, GLuint texture, GLenum access, GLenum format);

void bindOutputTexture(const std::string& name, GLuint texture, GLint level = 0);
void bindOutputRenderbuffer(const std::string& name, GLuint renderbuffer);
void bindOutputDepthTexture(GLuint texture, GLint level = 0);
void bindOutputDepthRenderbuffer(GLuint renderbuffer);

// todo: should the RAII classes be separated into another header? someone might want to use only one of the two
// not to be used directly ( todo: put these out of sight into a different header after finalized )
namespace detail {

	// Generic RAII lifetime handler for GLuint-based objects; in principle, very close to unique pointers (with GLuint playing the role of a raw pointer).
	// todo: make this CRTP instead? could be prettier, allow add special constructors more easily
	template<GLuint(create)(void), void(destroy)(GLuint)>
	struct GLObject {
	public:
		inline GLObject() { object = create(); }
		inline ~GLObject() { if(object!=0) destroy(object); }
		inline GLObject(const GLuint other) { object = other; }
		inline GLObject& operator=(const GLuint other) { if (object != 0) destroy(object); object = other; return *this; }
		inline GLObject(GLObject && other) { std::swap(object, other.object); }
		inline GLObject& operator=(GLObject && other) { std::swap(object, other.object); return *this; }
		inline operator GLuint() const { return object; }
		inline operator bool() const { return object != 0; }
	protected:
		GLuint object = 0;
	};

	// create/destroy functions for each type
	inline GLuint createProgram() { return GLuint(0); }
	inline void destroyProgram(GLuint o) { glDeleteProgram(o); }
	
	inline GLuint createFramebuffer() { GLuint o; glCreateFramebuffers(1, &o); return o; }
	inline void destroyFramebuffer(GLuint o) { glDeleteFramebuffers(1, &o); }

	inline GLuint createBuffer() { GLuint o; glCreateBuffers(1, &o); return o; }
	inline void destroyBuffer(GLuint o) { glDeleteBuffers(1, &o); }

	template<GLenum target>
	inline GLuint createTexture() { GLuint o; glCreateTextures(target, 1, &o); return o; }
	inline void destroyTexture(GLuint o) { glDeleteTextures(1, &o); }

	template<GLenum target>
	inline GLuint createRenderbuffer() { GLuint o; glCreateRenderbuffers(target, 1, &o); return o; }
	inline void destroyRenderbuffer(GLuint o) { glDeleteRenderbuffers(1, &o); }
}

// GLObject should be used via these aliases:
using Program = detail::GLObject<detail::createProgram, detail::destroyProgram>;
using Framebuffer = detail::GLObject <detail::createFramebuffer, detail::destroyFramebuffer>;
using Buffer = detail::GLObject < detail::createBuffer, detail::destroyBuffer>;
template<GLenum target> using Texture = detail::GLObject < detail::createTexture<target>, detail::destroyTexture>;
template<GLenum target> using Renderbuffer = detail::GLObject < detail::createRenderbuffer<target>, detail::destroyRenderbuffer>;


Texture<GL_TEXTURE_2D> loadImage(const std::string& path);
Texture<GL_TEXTURE_2D> loadImage(const std::wstring& path);

// todo: stringviews?
#define uniform_copy(postfix, postfix_v, type)\
void glUniform1##postfix(const std::string& name, type value);\
void glUniform2##postfix(const std::string& name, type value1, type value2);\
void glUniform3##postfix(const std::string& name, type value1, type value2, type value3);\
void glUniform4##postfix(const std::string& name, type value1, type value2, type value3, type value4);\
void glUniform1##postfix_v(const std::string& name, GLsizei count, const type* value);\
void glUniform2##postfix_v(const std::string& name, GLsizei count, const type* value);\
void glUniform3##postfix_v(const std::string& name, GLsizei count, const type* value);\
void glUniform4##postfix_v(const std::string& name, GLsizei count, const type* value);

uniform_copy(i, iv, GLint)
uniform_copy(ui, uiv, GLuint)
uniform_copy(f, fv, GLfloat)
#undef uniform_copy

#define uniform_matrix_copy(func) void glUniformMatrix##func(const std::string& name, GLsizei count, GLboolean transpose, const GLfloat *value);
uniform_matrix_copy(2fv)
uniform_matrix_copy(3fv)
uniform_matrix_copy(4fv)
uniform_matrix_copy(2x3fv)
uniform_matrix_copy(3x2fv)
uniform_matrix_copy(2x4fv)
uniform_matrix_copy(4x2fv)
uniform_matrix_copy(3x4fv)
uniform_matrix_copy(4x3fv)
#undef uniform_matrix_copy
