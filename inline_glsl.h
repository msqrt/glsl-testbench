#pragma once

#include <string>
#include <string_view>
#include <fstream>
#include <filesystem>

// debug
#include <iostream>

namespace detail {
	namespace {
		int glslCounter = 0;
	};
	struct inlineGLSL {
		std::string source;
		int index;
		inlineGLSL(const std::string_view path, const int version, std::string_view source, bool printSrc):
			index(glslCounter++),
			source("#version " + std::to_string(version) + "\n" + std::string(source))
		{
			using namespace std;
			namespace fs = std::filesystem;
			cout << path << ", " << version << endl;
			cout << fs::current_path() << endl;

			auto extract = [](string_view& str, const char delimiter)->string_view {
				while (str.length() > 0 && str[0] == delimiter)
					str = str.substr(1);
				size_t count = str.find_first_of(delimiter);
				if (count == string_view::npos) count = str.length();
				const string_view val = str.substr(0, count);
				str = str.substr(min(count + 1, str.length()));
				return val;
			};

			string file(istreambuf_iterator<char>(ifstream(path).rdbuf()), istreambuf_iterator<char>());
			

			
			// take file
			// go through
			// find GLSL

			//if (printSrc)
			//	cout << source << endl;
		}
		operator std::string() {
			return source;
		}
	};
}

#define GLSL(version, str) detail::inlineGLSL(__FILE__, version, #str, false)
#define GLSL_debug_print(version, str) detail::inlineGLSL(__FILE__, version, #str, true)

inline void dummyCompile(const std::string& str) {
	std::cout << str << std::endl;
}
