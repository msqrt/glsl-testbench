#pragma once

#include <Windows.h>

#include <vector>
#include <string>
#include <string_view>
#include <filesystem>

#include <gl/gl.h>
#include "loadgl/glext.h"
#include "loadgl/loadgl46.h"

#include "shaderprintf.h"

struct Program;

Program createProgram(const std::string_view computePath);
Program createProgram(const std::string_view vertexPath, const std::string_view controlPath, const std::string_view evaluationPath, const std::string_view geometryPath, const std::string_view fragmentPath);

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
		GLObject() { object = create(); }
		~GLObject() { if(object!=0) destroy(object); }
		GLObject(const GLuint other) { object = other; }
		GLObject& operator=(const GLuint other) { if (object != 0) destroy(object); object = other; return *this; }
		GLObject(GLObject && other) { std::swap(object, other.object); }
		GLObject& operator=(GLObject && other) { std::swap(object, other.object); return *this; }
		operator GLuint() const { return object; }
		operator bool() const { return object != 0; }
	protected:
		GLuint object = 0;
	};

	// create/destroy functions for each type

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
using Framebuffer = detail::GLObject <detail::createFramebuffer, detail::destroyFramebuffer>;
using Buffer = detail::GLObject < detail::createBuffer, detail::destroyBuffer>;
template<GLenum target> using Texture = detail::GLObject < detail::createTexture<target>, detail::destroyTexture>;
template<GLenum target> using Renderbuffer = detail::GLObject < detail::createRenderbuffer<target>, detail::destroyRenderbuffer>;

bool isOfType(const GLenum type, const GLenum* types, const GLint typeCount);

#include "inline_glsl.h"

// program: handles reloads
struct Program {
public:

	Program() {}
	~Program() { if (program != 0) destroy(); }

	Program& operator=(Program && other) {
		if (other.program == 0) return *this;
		std::swap(program, other.program);
		std::swap(filePaths, other.filePaths);
		std::swap(args, other.args);
		std::swap(lastLoad, other.lastLoad);
		return *this;
	}
	Program(Program && other) { *this = std::move(other); }

	operator GLuint() {
		reloadIfRequired();
		return program;
	}
	operator GLuint() const {
		return program;
	}
	
	// use these to create programs
	friend Program createProgram(const std::string_view computePath);
	friend Program createProgram(const std::string_view vertexPath, const std::string_view controlPath, const std::string_view evaluationPath, const std::string_view geometryPath, const std::string_view fragmentPath);

protected:


	// create a single shader object
	GLuint createShader(std::string_view path, const GLenum shaderType);

	// creates a compute program based on a single file
	Program(const std::string_view computePath);

	// creates a graphics program based on shaders for potentially all 5 programmable pipeline stages
	Program(
		const std::string_view vertexPath,
		const std::string_view controlPath,
		const std::string_view evaluationPath,
		const std::string_view geometryPath,
		const std::string_view fragmentPath);

	void reloadIfRequired();

	operator bool() const { return program != 0; }

	void addPath(std::string_view path);
	void destroy() {glDeleteProgram(program);}
	GLuint program = 0;
	std::vector<std::string> filePaths;
	std::vector<std::string> args;
	std::filesystem::file_time_type lastLoad;
};

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
