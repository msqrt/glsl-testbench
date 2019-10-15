#pragma once

#include <Windows.h>

#include <fstream>
#include <array>
#include <vector>
#include <iostream>


#include <string>
#include <filesystem>

#include <gl/gl.h>
#include "loadgl/glext.h"
#include "loadgl/loadgl46.h"

#include "shaderprintf.h"

struct Program;

Program createProgram(const std::string& computePath);
Program createProgram(const std::string& vertexPath, const std::string& controlPath, const std::string& evaluationPath, const std::string& geometryPath, const std::string& fragmentPath);

bool reloadRequired(GLuint program);

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

// program: handles reloads
struct Program {
public:
	GLenum samplerTypes[40] = { GL_SAMPLER_1D, GL_SAMPLER_2D, GL_SAMPLER_3D, GL_SAMPLER_CUBE, GL_SAMPLER_1D_SHADOW, GL_SAMPLER_2D_SHADOW, GL_SAMPLER_1D_ARRAY, GL_SAMPLER_2D_ARRAY, GL_SAMPLER_CUBE_MAP_ARRAY, GL_SAMPLER_1D_ARRAY_SHADOW,GL_SAMPLER_2D_ARRAY_SHADOW, GL_SAMPLER_2D_MULTISAMPLE,GL_SAMPLER_2D_MULTISAMPLE_ARRAY, GL_SAMPLER_CUBE_SHADOW, GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW, GL_SAMPLER_BUFFER, GL_SAMPLER_2D_RECT, GL_SAMPLER_2D_RECT_SHADOW, GL_INT_SAMPLER_1D, GL_INT_SAMPLER_2D, GL_INT_SAMPLER_3D, GL_INT_SAMPLER_CUBE, GL_INT_SAMPLER_1D_ARRAY, GL_INT_SAMPLER_2D_ARRAY, GL_INT_SAMPLER_CUBE_MAP_ARRAY, GL_INT_SAMPLER_2D_MULTISAMPLE, GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY, GL_INT_SAMPLER_BUFFER, GL_INT_SAMPLER_2D_RECT, GL_UNSIGNED_INT_SAMPLER_1D, GL_UNSIGNED_INT_SAMPLER_2D, GL_UNSIGNED_INT_SAMPLER_3D, GL_UNSIGNED_INT_SAMPLER_CUBE, GL_UNSIGNED_INT_SAMPLER_1D_ARRAY, GL_UNSIGNED_INT_SAMPLER_2D_ARRAY, GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY, GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE, GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY,GL_UNSIGNED_INT_SAMPLER_BUFFER, GL_UNSIGNED_INT_SAMPLER_2D_RECT };
	GLenum imageTypes[32] = { GL_IMAGE_1D, GL_IMAGE_2D, GL_IMAGE_3D, GL_IMAGE_2D_RECT, GL_IMAGE_CUBE, GL_IMAGE_BUFFER, GL_IMAGE_1D_ARRAY, GL_IMAGE_2D_ARRAY, GL_IMAGE_CUBE_MAP_ARRAY, GL_IMAGE_2D_MULTISAMPLE, GL_IMAGE_2D_MULTISAMPLE_ARRAY, GL_INT_IMAGE_1D, GL_INT_IMAGE_2D, GL_INT_IMAGE_3D, GL_INT_IMAGE_2D_RECT, GL_INT_IMAGE_CUBE, GL_INT_IMAGE_BUFFER, GL_INT_IMAGE_1D_ARRAY, GL_INT_IMAGE_2D_ARRAY, GL_INT_IMAGE_CUBE_MAP_ARRAY, GL_INT_IMAGE_2D_MULTISAMPLE, GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY, GL_UNSIGNED_INT_IMAGE_1D, GL_UNSIGNED_INT_IMAGE_2D, GL_UNSIGNED_INT_IMAGE_3D, GL_UNSIGNED_INT_IMAGE_CUBE, GL_UNSIGNED_INT_IMAGE_BUFFER, GL_UNSIGNED_INT_IMAGE_1D_ARRAY, GL_UNSIGNED_INT_IMAGE_2D_ARRAY, GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY, GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE, GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY };

	GLenum typeProperty = GL_TYPE;
	GLenum locationProperty = GL_LOCATION;

	bool isOfType(const GLenum type, const GLenum* types, const GLint typeCount) {
		for (int i = 0; i < typeCount; ++i)
			if (type == types[i])
				return true;
		return false;
	}

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

	std::string getFirstLine(const std::string& path) {
		return path.substr(0, path.find('\n'));
	}

	// create a single shader object
	GLuint createShader(const std::string& path, const GLenum shaderType) {
		using namespace std;

		// if there are line breaks, this is an inline shader instead of a file. if so, we skip the first line (it contains a name) and compile the rest. otherwise read file as source.
		const size_t search = path.find('\n');
		const string source = search != string::npos ? path.substr(path.find('\n', search + 1) + 1) : string(istreambuf_iterator<char>(ifstream(path).rdbuf()), istreambuf_iterator<char>());
		addPath(search != string::npos ? path.substr(search + 1, path.find('\n', search + 1) - search - 1) : path);

		const GLuint shader = glCreateShader(shaderType);
		auto source_ptr = (const GLchar*)source.c_str();

		glShaderSourcePrint(shader, 1, &source_ptr, nullptr);
		glCompileShader(shader);

		// print error log if failed to compile
		int success; glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success) {
			int length; glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
			string log(length + 1, '\0');
			glGetShaderInfoLog(shader, length + 1, &length, &log[0]);
			printf("log of compiling %s:\n%s\n", getFirstLine(path).c_str(), log.c_str());
		}
		return shader;
	}
	void assignUnits(const GLuint program, const GLenum* types, const GLint typeCount) {
		GLint unit = 0, location, type, count;
		glGetProgramInterfaceiv(program, GL_UNIFORM, GL_ACTIVE_RESOURCES, &count);
		for (int i = 0; i < count; ++i) {
			glGetProgramResourceiv(program, GL_UNIFORM, i, 1, &typeProperty, sizeof(type), nullptr, &type);
			if (isOfType((GLenum)type, types, typeCount)) {
				glGetProgramResourceiv(program, GL_UNIFORM, i, 1, &locationProperty, sizeof(location), nullptr, &location);
				glUniform1i(location, unit);
				unit++;
			}
		}
	}

	// creates a compute program based on a single file
	Program(const std::string& computePath) {
		using namespace std;
		program = glCreateProgram();

		args = { computePath };

		const GLuint computeShader = createShader(computePath, GL_COMPUTE_SHADER);
		glAttachShader(program, computeShader);
		glLinkProgram(program);
		glDeleteShader(computeShader);

		// print error log if failed to link
		int success; glGetProgramiv(program, GL_LINK_STATUS, &success);
		if (!success) {
			int length; glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
			string log(length + 1, '\0');
			glGetProgramInfoLog(program, length + 1, &length, &log[0]);
			printf("log of linking %s:\n%s\n", getFirstLine(computePath).c_str(), log.c_str());
			glDeleteProgram(program);
			return;
		}

		glUseProgram(program);
		assignUnits(program, samplerTypes, sizeof(samplerTypes) / sizeof(GLenum));
		assignUnits(program, imageTypes, sizeof(imageTypes) / sizeof(GLenum));
	}

	// sort all inline glsl sources; they're evaluated in an undefined order (since they're arguments) but the first differing character will be the index of the GLSL macro in the source code
	std::array<std::string, 5> sortInlineSources(std::array<std::string, 5>&& paths) {
		std::array<int, 5> inds;
		for (int i = 0; i < 5; ++i)
			inds[i] = (paths[i].length() > 0 && paths[i].find('\n') != std::string::npos) ? i : -1;

		std::sort(inds.begin(), inds.end(), [&](int a, int b) {if (a == -1) return false; if (b == -1) return true; return paths[a] < paths[b]; });

		auto result = paths;
		int index = 0;
		for (auto& path : result)
			if (path.length() > 0 && path.find('\n') != std::string::npos)
				path = paths[inds[index++]];
		return result;
	}

	// creates a graphics program based on shaders for potentially all 5 programmable pipeline stages
	Program(const std::string& vertexPath, const std::string& controlPath, const std::string& evaluationPath, const std::string& geometryPath, const std::string& fragmentPath) {
		using namespace std;

		args = { vertexPath, controlPath, evaluationPath, geometryPath, fragmentPath };

		program = glCreateProgram();
		const array<string, 5> paths = sortInlineSources({ vertexPath, controlPath, evaluationPath, geometryPath, fragmentPath });
		const GLenum types[] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER };
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
			printf("log of linking (%s,%s,%s,%s,%s):\n%s\n",
				getFirstLine(vertexPath).c_str(),
				getFirstLine(controlPath).c_str(),
				getFirstLine(evaluationPath).c_str(),
				getFirstLine(geometryPath).c_str(),
				getFirstLine(fragmentPath).c_str(),
				log.c_str());
			glDeleteProgram(program);
			return;
		}

		glUseProgram(program);
		assignUnits(program, samplerTypes, sizeof(samplerTypes) / sizeof(GLenum));
		assignUnits(program, imageTypes, sizeof(imageTypes) / sizeof(GLenum));
	}

	void reloadIfRequired() {
		for (int j = 0; j < filePaths.size(); ++j) {
			std::error_code ec;
			auto fileModify = std::filesystem::last_write_time(std::filesystem::path(filePaths[j]), ec);
			if (!ec && fileModify > lastLoad) {
				lastLoad = fileModify;
				std::cout << "before" << program << std::endl;
				if (args.size() == 1)
					*this = std::move(Program(args[0]));
				else
					*this = std::move(Program(args[0], args[1], args[2], args[3], args[4]));
				std::cout << "after" << program << std::endl;
				return;
			}
		}
	}
	operator GLuint() {
		reloadIfRequired();
		return program;
	}
	operator GLuint() const {
		return program;
	}

	operator bool() const { return program != 0; }

	void reset() { if (program != 0) destroy(); program = 0; filePaths.clear(); lastLoad = {}; }
	void addPath(const std::string& path) {
		if (path.length() > 0 && std::find(filePaths.begin(), filePaths.end(), path)!=filePaths.end()) return;

		filePaths.push_back(path);
		std::error_code ec;
		auto fileModify = std::filesystem::last_write_time(std::filesystem::path(path), ec);
		if (!ec && fileModify > lastLoad)
			lastLoad = fileModify;
	}
protected:
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
