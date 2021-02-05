
#include "gl_helpers.h"

#include <iostream>
#include <fstream>
#include <array>
#include <charconv>
#include <algorithm>

// whenever a shader is created, we go through all of its uniforms and assign unit indices for textures and images.
const GLenum samplerTypes[] = { GL_SAMPLER_1D, GL_SAMPLER_2D, GL_SAMPLER_3D, GL_SAMPLER_CUBE, GL_SAMPLER_1D_SHADOW, GL_SAMPLER_2D_SHADOW, GL_SAMPLER_1D_ARRAY, GL_SAMPLER_2D_ARRAY, GL_SAMPLER_CUBE_MAP_ARRAY, GL_SAMPLER_1D_ARRAY_SHADOW,GL_SAMPLER_2D_ARRAY_SHADOW, GL_SAMPLER_2D_MULTISAMPLE,GL_SAMPLER_2D_MULTISAMPLE_ARRAY, GL_SAMPLER_CUBE_SHADOW, GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW, GL_SAMPLER_BUFFER, GL_SAMPLER_2D_RECT, GL_SAMPLER_2D_RECT_SHADOW, GL_INT_SAMPLER_1D, GL_INT_SAMPLER_2D, GL_INT_SAMPLER_3D, GL_INT_SAMPLER_CUBE, GL_INT_SAMPLER_1D_ARRAY, GL_INT_SAMPLER_2D_ARRAY, GL_INT_SAMPLER_CUBE_MAP_ARRAY, GL_INT_SAMPLER_2D_MULTISAMPLE, GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY, GL_INT_SAMPLER_BUFFER, GL_INT_SAMPLER_2D_RECT, GL_UNSIGNED_INT_SAMPLER_1D, GL_UNSIGNED_INT_SAMPLER_2D, GL_UNSIGNED_INT_SAMPLER_3D, GL_UNSIGNED_INT_SAMPLER_CUBE, GL_UNSIGNED_INT_SAMPLER_1D_ARRAY, GL_UNSIGNED_INT_SAMPLER_2D_ARRAY, GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY, GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE, GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY,GL_UNSIGNED_INT_SAMPLER_BUFFER, GL_UNSIGNED_INT_SAMPLER_2D_RECT };
const GLenum imageTypes[] = { GL_IMAGE_1D, GL_IMAGE_2D, GL_IMAGE_3D, GL_IMAGE_2D_RECT, GL_IMAGE_CUBE, GL_IMAGE_BUFFER, GL_IMAGE_1D_ARRAY, GL_IMAGE_2D_ARRAY, GL_IMAGE_CUBE_MAP_ARRAY, GL_IMAGE_2D_MULTISAMPLE, GL_IMAGE_2D_MULTISAMPLE_ARRAY, GL_INT_IMAGE_1D, GL_INT_IMAGE_2D, GL_INT_IMAGE_3D, GL_INT_IMAGE_2D_RECT, GL_INT_IMAGE_CUBE, GL_INT_IMAGE_BUFFER, GL_INT_IMAGE_1D_ARRAY, GL_INT_IMAGE_2D_ARRAY, GL_INT_IMAGE_CUBE_MAP_ARRAY, GL_INT_IMAGE_2D_MULTISAMPLE, GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY, GL_UNSIGNED_INT_IMAGE_1D, GL_UNSIGNED_INT_IMAGE_2D, GL_UNSIGNED_INT_IMAGE_3D, GL_UNSIGNED_INT_IMAGE_CUBE, GL_UNSIGNED_INT_IMAGE_BUFFER, GL_UNSIGNED_INT_IMAGE_1D_ARRAY, GL_UNSIGNED_INT_IMAGE_2D_ARRAY, GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY, GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE, GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY };

bool isOfType(const GLenum type, const GLenum* types, const GLint typeCount) {
	for (int i = 0; i < typeCount; ++i)
		if (type == types[i])
			return true;
	return false;
}

const GLenum typeProperty = GL_TYPE;
const GLenum locationProperty = GL_LOCATION;

// helper; read until the first newline (or to the end)
std::string_view getFirstLine(std::string_view path) {
	return path.substr(0, path.find('\n'));
}

// adds a filepath to the set of watched files for reloading
void Program::addPath(std::string_view path) {
	if (path.length() > 0 && std::find(filePaths.begin(), filePaths.end(), path) != filePaths.end()) return;

	// needs to be added; also update the load time
	filePaths.push_back(std::string(path));
	std::error_code ec;
	auto fileModify = std::filesystem::last_write_time(std::filesystem::path(path), ec);
	if (!ec && fileModify > lastLoad)
		lastLoad = fileModify;
}

// check if any of the files has changed; reload if so
void Program::reloadIfRequired() {
	for (auto& path : filePaths) {
		std::error_code ec;
		auto fileModify = std::filesystem::last_write_time(std::filesystem::path(path), ec);
		if (!ec && fileModify > lastLoad) {
			if (args.size() == 1)
				*this = std::move(Program(args[0]));
			else
				*this = std::move(Program(args[0], args[1], args[2], args[3], args[4]));
			return;
		}
	}
}

GLuint Program::createShader(std::string_view path, const GLenum shaderType) {
	using namespace std;

	// if there are line breaks, this is an inline shader instead of a file. if so, we skip the first line (it contains a name) and compile the rest. otherwise read file as source.
	const size_t search = path.find('\n');
	const string source = search != string::npos ? string(path.substr(path.find('\n', search + 1) + 1)) : string(istreambuf_iterator<char>(ifstream(path).rdbuf()), istreambuf_iterator<char>());
	addPath(search != string::npos ? path.substr(search + 1, path.find('\n', search + 1) - search - 1) : path);

	const GLuint shader = glCreateShader(shaderType);
	auto source_ptr = (const GLchar*)source.data();
	const GLint source_len = GLint(source.length());

	glShaderSourcePrint(shader, 1, &source_ptr, &source_len);
	glCompileShader(shader);

	// print error log if failed to compile
	int success; glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		int length; glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
		string log(length + 1, '\0');
		glGetShaderInfoLog(shader, length + 1, &length, &log[0]);
		cout << "log of compiling " << getFirstLine(path) << ":\n" << log << "\n";
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

// counts the amount of objects of the same type before this one in the arbitrary order the API happens to give them; this gives a unique index for each object that's used for the texture and image unit
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

bool isnamechar(const uint8_t c) { return std::isalnum(c) || c == '_'; }

std::string_view extract(std::string_view& str, const char delimiter) {
	size_t count = str.find_first_of(delimiter);
	if (count == std::string_view::npos) count = str.length();
	const std::string_view val = str.substr(0, count);
	str = str.substr(min(count + 1, str.length()));
	return val;
};

template<typename T>
T charconv(const std::string_view str, T value) {
	std::from_chars(str.data(), str.data() + str.size(), value);
	return value;
};

inline std::string getGLSLcode(std::string_view path_or_source) {
	using namespace std;

	if (path_or_source.length() == 0) return "";

	if (path_or_source.find('\n') == string_view::npos) // path
		return string(path_or_source);

	string_view path = extract(path_or_source, ',');
	const int index = charconv(extract(path_or_source, ','), int{});
	const int version = charconv(extract(path_or_source, '\n'), int{});

	// can't remove whitespace between letters, but can between letters and other characters

	// removes comments and extra white space from a code file
	// comments are C-style; // /**/. variable names can contain A-Z,a-z,0-9 and underscores.
	// all whitespace is removed if it's not surrounded by possible variable names from both sides.
	auto simplifySourceFile = [](string_view fileString, bool removeStringLiterals = false) {
		string parsed;
		parsed.reserve(fileString.length());

		bool multilineComment = false;
		bool singleLineComment = false;
		bool isStringLiteral = false;
		uint8_t before_spaces = 0;
		uint8_t prev = '\n'; // treat first line as if there was a preceding one
		for (uint8_t c : fileString) {
			uint8_t out = 0;

			if (c == '\n') { // always preserve lines
				out = c;
				before_spaces = 0;
				singleLineComment = false;
			}
			else if (isStringLiteral && removeStringLiterals && !(c == '\"'&&prev != '\\')); // remove if we're inside a string literal (but not the ends; keep the code valid)
			else if (singleLineComment); // the rest of the line is commented away
			else if (multilineComment)
				multilineComment = !(prev == '*' && c == '/');
			else if (prev == '/' && c == '/') {
				parsed.pop_back();
				singleLineComment = true;
			}
			else if (prev == '/' && c == '*') {
				parsed.pop_back();
				multilineComment = true;
			}
			else if (std::isspace(c)) {
				if (!std::isspace(prev))
					before_spaces = prev;
			}
			else if (!std::isspace(c) && std::isspace(prev)) {
				if (isnamechar(c) && isnamechar(before_spaces))
					parsed.push_back(' ');
				out = c;
			}
			else
				out = c;
			if (c == '\"' && prev != '\\')
				isStringLiteral = !isStringLiteral;

			if (out != 0)
				parsed.push_back(out);

			prev = c;
		}
		return parsed;
	};

	auto findMarker = [](string_view path, string_view fileString, int index) {
		size_t findIndex = 0;
		while (true) {
			findIndex = fileString.find("GLSL", findIndex);
			if (findIndex == string::npos) {
				cout << "couldn't locate GLSL string " << index << " in file " << path << "\n";
				return std::make_pair(string::npos, string::npos);
			}
			else if ((findIndex <= 0 || !isnamechar(fileString[findIndex - 1])) && // not "somethingGLSL"
				(findIndex + 4 >= fileString.size() || !isnamechar(fileString[findIndex + 4])) && // not "GLSLsomething"
				--index < 0) // the correct index
				for (size_t i = findIndex + 4; i < fileString.size(); i++)
					if (fileString[i] == ',')
						return std::make_pair(i + 1, findIndex);
			findIndex++;
		}
	};

	auto file = string(istreambuf_iterator<char>(ifstream(path).rdbuf()), istreambuf_iterator<char>());
	string parsed = simplifySourceFile(file, true);

	auto searchResult = findMarker(path, parsed, index);
	size_t glslIndex = searchResult.first;
	size_t macroLocation = searchResult.second;
	int parens = 1;
	string shaderSource;
	if (glslIndex != string::npos) {
		shaderSource.reserve(parsed.size() - glslIndex);

		for (int i = int(glslIndex); i < parsed.size(); ++i) {
			if (parsed[i] == '(') parens++;
			if (parsed[i] == ')') parens--;
			if (parens == 0) break;
			shaderSource.push_back(parsed[i]);
		}
	}
	auto formatSource = [&](const std::string& path, const auto macro_line, const auto version, const auto source_line, const std::string& src) {
		return "GLSL(" + std::to_string(index) + ") at " + std::filesystem::path(path).filename().string() + ", line " + std::to_string(macro_line) + "\n" + path + "\n" + "#version " + std::to_string(version) + "\n#line " + std::to_string(source_line) + "\n" + src;
	};

	if (parens > 0) // eof..??
		return " broken GLSL(" + to_string(index) + ") in file " + std::filesystem::path(path).filename().string() + ", compiling given string instead\n" + string(path_or_source);
	else {
		size_t macro_line = 1 + count(parsed.begin(), parsed.begin() + macroLocation, '\n');
		size_t source_line = 1 + count(parsed.begin(), parsed.begin() + glslIndex, '\n');

		return formatSource(string(path), macro_line, version, source_line, shaderSource);
	}
}

Program::Program(const std::string_view computePath) {
	using namespace std;

	args = { string(computePath) };

	const GLuint computeShader = createShader(getGLSLcode(computePath), GL_COMPUTE_SHADER);
	if (!computeShader) {
		destroy();
		return;
	}

	program = glCreateProgram();
	glAttachShader(program, computeShader);
	glLinkProgram(program);
	glDeleteShader(computeShader);

	// print error log if failed to link
	int success; glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		int length; glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
		string log(length + 1, '\0');
		glGetProgramInfoLog(program, length + 1, &length, &log[0]);
		cout << "log of linking " << getFirstLine(computePath) << ":\n" << log << "\n";
		glDeleteProgram(program);
		destroy();
		return;
	}

	glUseProgram(program);
	assignUnits(program, samplerTypes, sizeof(samplerTypes) / sizeof(GLenum));
	assignUnits(program, imageTypes, sizeof(imageTypes) / sizeof(GLenum));
}

// sort all inline glsl sources; they're evaluated in an undefined order (since they're arguments)
std::array<std::string_view, 5> sortInlineSources(std::array<std::string_view, 5>&& paths) {
	std::array<int, 5> inds;
	for (int i = 0; i < 5; ++i)
		inds[i] = (paths[i].length() > 0 && paths[i].find('\n') != std::string::npos) ? i : -1;

	std::array<int, 5> glslIndices;
	for (size_t i = 0; i < glslIndices.size(); i++)
	{
		glslIndices[i] = -1;
		std::string_view p = paths[i];
		extract(p, ',');
		glslIndices[i] = charconv(extract(p, ','), glslIndices[i]);
	}

	auto predicate = [&](int a, int b) {
		if (a == -1) return false;
		if (b == -1) return true;
		if (glslIndices[a] == -1) return false;
		if (glslIndices[b] == -1) return true;
		return glslIndices[a] < glslIndices[b];
	};

	std::sort(inds.begin(), inds.end(), predicate);

	auto result = paths;
	int index = 0;
	for (auto& path : result)
		if (path.length() > 0 && path.find('\n') != std::string::npos)
			path = paths[inds[index++]];
	return result;
}

Program::Program(const std::string_view vertexPath, const std::string_view controlPath, const std::string_view evaluationPath, const std::string_view geometryPath, const std::string_view fragmentPath) {
	using namespace std;

	args = { string(vertexPath), string(controlPath), string(evaluationPath), string(geometryPath), string(fragmentPath) };

	program = glCreateProgram();
	const array<string_view, 5> paths = sortInlineSources({ vertexPath, controlPath, evaluationPath, geometryPath, fragmentPath });
	const GLenum types[] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER };

	array<string, 5> path_or_source;
	for (int i = 0; i < 5; ++i)
		path_or_source[i] = getGLSLcode(paths[i]);

	bool compileOk = true;
	for (int i = 0; i < 5; ++i) {
		if (!paths[i].length()) continue;
		GLuint shader = createShader(path_or_source[i], types[i]);
		if (!shader) {
			compileOk = false;
			continue;
		}
		glAttachShader(program, shader);
		glDeleteShader(shader); // deleting here is okay; the shader object is reference-counted with the programs it's attached to
	}
	if (!compileOk) {
		destroy();
		return;
	}
	glLinkProgram(program);

	// print error log if failed to link
	int success; glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		int length; glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
		string log(length + 1, '\0');
		glGetProgramInfoLog(program, length + 1, &length, &log[0]);
		cout << "log of linking "
			<< getFirstLine(path_or_source[0])
			<< getFirstLine(path_or_source[1])
			<< getFirstLine(path_or_source[2])
			<< getFirstLine(path_or_source[3])
			<< getFirstLine(path_or_source[4]) << ":\n" << log << "\n";
		destroy();
		return;
	}

	glUseProgram(program);
	assignUnits(program, samplerTypes, sizeof(samplerTypes) / sizeof(GLenum));
	assignUnits(program, imageTypes, sizeof(imageTypes) / sizeof(GLenum));
}
