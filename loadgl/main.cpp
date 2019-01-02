
#include <string>
#include <iostream>
#include <cctype>
#include <fstream>
#include <sstream>
#include <locale>
#include <vector>
#include <set>
#include <algorithm>
#include <unordered_set>

int main(int argc, char* argv[]) {

	if (argc < 2) {
		std::cout << "usage: parseglexth version [ext ext ext ...]" << std::endl
			<< "\"version\" is the desired OpenGL version number." << std::endl
			<< "all of the specified extensions will also be loaded." << std::endl;
		system("pause");
		return -1;
	}

	std::string maxver_str = std::string(argv[1]);
	maxver_str.erase(std::remove(maxver_str.begin(), maxver_str.end(), '.'), maxver_str.end());

	int maxver = std::atoi(maxver_str.c_str());

	std::vector<std::string> extensions;
	for (int i = 2; i < argc; ++i)
		extensions.push_back(argv[i]);

	std::ifstream glext("glext.h");
	std::stringstream define;
	std::stringstream declare;
	std::stringstream load;
	std::stringstream unload;
	std::stringstream load_check;
	std::stringstream load_debug;
	load << "int loadgl() {" << std::endl;
	unload << "void unloadgl() {" << std::endl;
	load_check << "\t" << "if(" << std::endl;
	load_debug << "int loadgl() {" << std::endl << "\t" << "int result = 0;" << std::endl;

	std::string line;

	std::locale loc;

	std::set<std::string> funcnames;

	while (std::getline(glext, line)) {
		int start_parse = 0;
		std::string cur_version = "";
		if (std::string::npos != line.find("#ifndef GL_VERSION_")) {
			int major = std::stoi(line.substr(line.rfind("_") - 1, 1)),
				minor = std::stoi(line.substr(line.rfind("_") + 1, 1));

			if (major * 10 + minor <= maxver)
				start_parse = 1;
			cur_version = "core version " + std::to_string(major) + "." + std::to_string(minor);

		}

		for (std::string& str : extensions)
			if (std::string::npos != line.find("#ifndef GL_ARB_" + str) || std::string::npos != line.find("#ifndef GL_EXT_" + str) || std::string::npos != line.find("#ifndef GL_KHR_" + str) || std::string::npos != line.find("#ifndef GL_AMD_" + str)) {
				cur_version = "extension " + str;
				start_parse = 2;
			}

		if (start_parse) {
			std::cout << std::endl << std::endl << "loading " << cur_version << std::endl;
			int inside = 0;
			while (std::getline(glext, line) && inside >= 0) {
				if (std::string::npos != line.find("#endif"))
					--inside;
				if (std::string::npos != line.find("#if"))
					++inside;
				if (std::string::npos != line.find("#ifdef GL_GLEXT_PROTOTYPES")) {

					while (std::getline(glext, line) && std::string::npos == line.find("#endif")) {
						int firstpos = line.find("APIENTRY ") + std::string("APIENTRY ").length();
						int lastpos = line.find("(") - firstpos;
						std::string spacedfunc = line.substr(firstpos, lastpos), func;
						std::string args = line.substr(line.find("(")+1, line.find(")")- line.find("(")-1), stripargs;
						std::string retval = line.substr(line.find("GLAPI ") + 6, line.find("APIENTRY ") - (line.find("GLAPI ") + 6));

						for (auto& f : spacedfunc)
							if (f != ' ')
								func += std::string(1,f);

						std::string cur;
						for (auto& a : args) {
							if (a == ' ' || a == '*') cur = "";
							else if (a == ',') { stripargs += cur + ", "; cur = ""; }
							else cur += a;
						}
						stripargs += cur;
						if (!stripargs.compare("void")) stripargs = "";

						std::cout << retval << " " << func << "(" << args << ");" << func << "(" << stripargs << ")" << "\n";

						std::string stripfunc = func;

						if (func.find("ARB") != std::string::npos)
							stripfunc = func.substr(0, func.find("ARB"));

						if (func.find("EXT") != std::string::npos)
							stripfunc = func.substr(0, func.find("EXT"));

						if (func.find("KHR") != std::string::npos)
							stripfunc = func.substr(0, func.find("KHR"));

						if (func.find("AMD") != std::string::npos)
							stripfunc = func.substr(0, func.find("AMD"));

						if (func.find("NV") != std::string::npos)
							stripfunc = func.substr(0, func.find("NV"));

						if (funcnames.count(stripfunc) == 0) {
							funcnames.insert(stripfunc);

							std::string proc = func;
							for (char& c : proc)
								c = std::toupper(c, loc);

							proc = "PFN" + proc + "PROC";

							define << "extern " << proc << " ptr_" << stripfunc << "; " << retval << " " << stripfunc << "(" << args << ");" << std::endl;
							// extern PFNGLBLENDCOLORPROC ptr_glBlendColor; ... glBlendColor(...);
							declare << proc << " ptr_" << stripfunc << " = nullptr; " << retval << " " << stripfunc << "(" << args << ") {" << (retval.substr(0,4).compare("void") ? " return" : "") << " ptr_" << stripfunc << "(" << stripargs << "); }" << std::endl;
							// PFNGLBLENDCOLORPROC ptr_glBlendColor = nullptr;
							unload << "\tptr_" << stripfunc << " = nullptr;" << std::endl;
							// ptr_glBlendColor = nullptr;
							load << "\tptr_" << stripfunc << " = (" << proc << ")wglGetProcAddress(\"" << func << "\");" << std::endl;
							// ptr_glBlendColor = (PFNGLBLENDCOLORPROC)wglGetProcAddress("glBlendColor");
							load_check << "\t\t\tptr_" << stripfunc << " == nullptr ||" << std::endl;
							load_debug << "\tptr_" << stripfunc << " = (" << proc << ")wglGetProcAddress(\"" << func << "\");" << std::endl;
							load_debug << "\tif(!ptr_" << stripfunc << ") { MessageBox(0, \"Couldn't load OpenGL " + cur_version + ", function \\\"" + func + "\\\" is missing.\", \"OpenGL function missing\" , MB_OK); result = -1; }" << std::endl << std::endl;
						}
					}
					--inside;
				}
			}
		}
	}
	load_check << "\t\t\t" << "false ) {" << std::endl << "\t\tMessageBox(0, \"Couldn't load OpenGL " + std::to_string(maxver / 10) + "." + std::to_string(maxver % 10);
	if (extensions.size() > 0) {
		if (extensions.size() > 1)
			load_check << ", ";
		else
			load_check << " and ";
		for (unsigned int i = 0; i < extensions.size(); ++i) {
			load_check << extensions[i];
			if (extensions.size()>1 && i<extensions.size() - 1)
				if (i == extensions.size() - 2)
					load_check << " and ";
				else
					load_check << ", ";
		}
	}
	load_check << "!\", \"OpenGL function missing\" , MB_OK); " << std::endl << " \t\treturn -1;" << std::endl << "\t" << "}" << std::endl;

	//while (std::getline(load_check, line))
	//	load << line << std::endl;

	load << "\t" << "return 0; " << std::endl << "}" << std::endl;
	unload << "}";
	load_debug << "\t" << "return result; " << std::endl << "}" << std::endl;

	std::ofstream cpp("loadgl" + std::to_string(maxver) + ".cpp");
	std::ofstream header("loadgl" + std::to_string(maxver) + ".h");
	header << "#pragma once" << std::endl << "int loadgl();" << std::endl << "void unloadgl();" << std::endl;
	while (std::getline(define, line))
		header << line << std::endl;
	cpp << "#include <Windows.h>" << std::endl;
	cpp << "#include <gl/gl.h>" << std::endl;
	cpp << "#include \"glext.h\"" << std::endl;
	cpp << "#include \"loadgl" + std::to_string(maxver) + ".h\"" << std::endl;
	while (std::getline(declare, line))
		cpp << line << std::endl;
	//cpp << "#ifdef _DEBUG" << std::endl;
	while (std::getline(load_debug, line))
		cpp << line << std::endl;
	//cpp << "#else" << std::endl;
	//while (std::getline(load, line))
	//	cpp << line << std::endl;
	//cpp << "#endif" << std::endl << std::endl;
	//while (std::getline(unload, line))
	//	cpp << line << std::endl;

	return 0;
}