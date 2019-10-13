#pragma once

#include <string>
#include <string_view>
#include <fstream>
#include <filesystem>
#include <cctype>
// debug
#include <iostream>


namespace detail {
	namespace {
		constexpr int base = __COUNTER__+1;
		bool isnamechar(const uint8_t c) { return std::isalnum(c) || c == '_'; }
	};

	struct inlineGLSL {
		std::string source;
		int index;

		std::string formatSource(const std::string& path, const int macro_line, const int version, const int source_line, const std::string& src) {
			return "GLSL(" + std::to_string(index) + ") at " + std::filesystem::path(path).filename().string() + ", line " + std::to_string(macro_line) + "\n" + "#version " + std::to_string(version) + "\n#line " + std::to_string(source_line) + "\n" + src;
		}

		inlineGLSL(const std::string& path, const int line, const int counter, const int version, std::string_view given_source, bool printSrc):
			source(formatSource(path,line,version,line,std::string(given_source)))
		{
			index = counter - base;
			using namespace std;

			// can't remove whitespace between letters, but can between letters and other characters

			// removes comments and extra white space from a code file
			// comments are C-style; // /**/. variable names can contain A-Z,a-z,0-9 and underscores.
			// all whitespace is removed if it's not surrounded by possible variable names from both sides.
			auto simplifySourceFile = [](const string& fileString, bool removeStringLiterals = false) {
				string parsed;
				parsed.reserve(fileString.size());

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
					else if (isStringLiteral && removeStringLiterals && !(c=='\"'&&prev!='\\')); // remove if we're inside a string literal (but not the ends; keep the code valid)
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
					if(c=='\"' && prev!='\\')
						isStringLiteral = !isStringLiteral;

					if (out != 0)
						parsed.push_back(out);

					prev = c;
				}
				return parsed;
			};

			auto findGlsl = [](const string& path, const string& fileString, int index) {
				size_t findIndex = 0;
				while (true) {
					findIndex = fileString.find("GLSL", findIndex);
					if (findIndex == string::npos) {
						printf("couldn't locate GLSL string %d in file %s", index, path.c_str());
						return std::make_pair(string::npos, string::npos);
					}
					else if (findIndex > 0 && !isnamechar(fileString[findIndex - 1]) && // not "somethingGLSL"
						findIndex + 4 < fileString.size() && !isnamechar(fileString[findIndex + 4]) && // not "GLSLsomething"
						--index < 0) // the correct index
						for (size_t i = findIndex + 4; i < fileString.size(); i++)
							if (fileString[i] == ',')
								return std::make_pair(i + 1, findIndex);
					findIndex++;
				}
			};

			auto file = string(istreambuf_iterator<char>(ifstream(path).rdbuf()), istreambuf_iterator<char>());
			string parsed = simplifySourceFile(file, true);

			/*auto extract = [](string_view& str, const char delimiter)->string_view {
				size_t count = str.find_first_of(delimiter);
				if (count == string_view::npos) count = str.length();
				const string_view val = str.substr(0, count);
				str = str.substr(min(count + 1, str.length()));
				return val;
			};

			string_view fv = file, pv = parsed;
			while (fv.length() > 0) {
				for (int i = 0; i < 10; ++i) {
					cout << extract(pv, '\n') << "#######" << extract(fv, '\n') << endl << endl;
				}
				system("pause");
			}*/

			auto searchResult = findGlsl(path, parsed, index);
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
			if (parens > 0)
				cout << "GLSL string " << to_string(index) << "broken, compiling given string instead\n";
			else {
				int macro_line = 1 + count(parsed.begin(), parsed.begin() + macroLocation, '\n');
				int source_line = 1 + count(parsed.begin(), parsed.begin() + glslIndex, '\n');
				source = formatSource(path, macro_line, version, source_line, shaderSource);
			}
			if (printSrc)
				cout << source;
		}
		operator std::string() {
			return source;
		}
	};
}

#define GLSL(version, str) detail::inlineGLSL(__FILE__, __LINE__, __COUNTER__, version, #str, false)
#define GLSL_debug_print(version, str) detail::inlineGLSL(__FILE__, __LINE__, __COUNTER__, version, #str, true)

inline void dummyCompile(const std::string& str) {
	std::cout << str << std::endl;
}
