#include "window.h"
#include "shaderprintf.h"
#include "gl_helpers.h"
#include "math_helpers.h"
#include "gl_timing.h"
#include "math.hpp"
#include "text_renderer.h"

#include <vector>
#include <random>
#include <fstream>
#include <string>
#include <string_view>
#include <charconv>

const int screenw = 1280, screenh = 720;

void loadObject();
std::vector<float> positions;
std::vector<uint32_t> indices;
std::vector<uint32_t> material_indices;
std::vector<float> material_albedo;

int main() {
	OpenGL context(screenw, screenh, "example");
	Font font(L"Consolas");

	Program objRender = createProgram("shaders/objVert.glsl", "", "", "shaders/objGeom.glsl", "shaders/objFrag.glsl");
	Program blit = createProgram("shaders/blitVert.glsl", "", "", "", "shaders/blitFrag.glsl");

	loadObject();

	Buffer positionBuffer, indexBuffer, materialIndexBuffer, materialAlbedoBuffer;
	glNamedBufferStorage(positionBuffer, sizeof(float) * positions.size(), positions.data(), 0);
	glNamedBufferStorage(indexBuffer, sizeof(float) * indices.size(), indices.data(), 0);
	glNamedBufferStorage(materialIndexBuffer, sizeof(float) * material_indices.size(), material_indices.data(), 0);
	glNamedBufferStorage(materialAlbedoBuffer, sizeof(float) * material_albedo.size(), material_albedo.data(), 0);

	Framebuffer fbo;
	Texture<GL_TEXTURE_2D> albedo, normal, depth;
	glTextureStorage2D(albedo, 1, GL_RGB16F, screenw, screenh);
	glTextureStorage2D(normal, 1, GL_RGB16F, screenw, screenh);
	glTextureStorage2D(depth, 1, GL_DEPTH_COMPONENT32F, screenw, screenh);

	auto logo = loadImage("assets/aalto.png");

	std::array<float, 16> camToClip;
	setupProjection(camToClip.data(), 1.f, screenw / float(screenh), .1f, 4.f);

	LARGE_INTEGER start, frequency;
	QueryPerformanceCounter(&start);
	QueryPerformanceFrequency(&frequency);

	while (loop()) // stops if esc pressed or window closed
	{
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		float t = float(double(now.QuadPart - start.QuadPart) / double(frequency.QuadPart));

		TimeStamp start;

		glUseProgram(objRender);

		// object binds
		bindBuffer("positionBuffer", positionBuffer);
		bindBuffer("materialIndexBuffer", materialIndexBuffer);
		bindBuffer("materialAlbedoBuffer", materialAlbedoBuffer);
		bindTexture("logo", logo);
		glUniformMatrix4fv("camToClip", 1, false, camToClip.data());
		glUniform1f("t", t);

		// multiple rendertargets
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		bindOutputTexture("color", albedo);
		bindOutputTexture("normal", normal);
		bindOutputDepthTexture(depth);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// state changes (classic opengl)
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
		glEnable(GL_DEPTH_TEST);

		glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, nullptr);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glDisable(GL_DEPTH_TEST);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, screenw, screenh); // viewport has to be set by hand :/

		glUseProgram(blit);
		bindTexture("albedo", albedo);
		bindTexture("normal", normal);
		bindTexture("depth", depth);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		TimeStamp end;

		font.drawText(L"⏱: " + std::to_wstring(end - start), 10.f, 10.f, 15.f); // wstring, x, y, font size
		swapBuffers();
	}
	return 0;
}

#include <charconv>

void loadObject()
{
	using namespace std;
	auto extract = [](string_view& str, const char delimiter)->string_view {
		while (str.length() > 0 && str[0] == delimiter)
			str = str.substr(1);
		size_t count = str.find_first_of(delimiter);
		if (count == string_view::npos) count = str.length();
		const string_view val = str.substr(0, count);
		str = str.substr(min(count + 1, str.length()));
		return val;
	};
	auto convert = [](std::string_view str, auto value) -> decltype(value) {
		from_chars(str.data(), str.data() + str.size(), value);
		return value;
	};
	vector<string> mtlNames;
	string mtlfile(istreambuf_iterator<char>(ifstream("assets/conference.mtl").rdbuf()), istreambuf_iterator<char>());
	string_view mtl_view = mtlfile;
	float r, g, b;
	while (mtl_view.length() > 0)
	{
		string_view line = extract(mtl_view, '\n');
		string_view identifier = extract(line, ' ');
		if (identifier == "newmtl")
		{
			if (mtlNames.size() > 0)
			{
				material_albedo.push_back(r);
				material_albedo.push_back(g);
				material_albedo.push_back(b);
				material_albedo.push_back(1.);
			}
			mtlNames.push_back(string(line));
		}
		else if (identifier == "Kd")
		{
			r = convert(extract(line, ' '), .0f);
			g = convert(extract(line, ' '), .0f);
			b = convert(extract(line, ' '), .0f);
		}
	}
	material_albedo.push_back(r);
	material_albedo.push_back(g);
	material_albedo.push_back(b);
	material_albedo.push_back(1.);

	string file(istreambuf_iterator<char>(ifstream("assets/conference.obj").rdbuf()), istreambuf_iterator<char>());
	string_view file_view = file;
	uint32_t use_mtl_index = 0;
	while (file_view.length() > 0)
	{
		string_view line = extract(file_view, '\n');
		string_view identifier = extract(line, ' ');
		if (identifier == "v")
		{
			for (int i = 0; i < 3; ++i)
				positions.push_back(convert(extract(line, ' '), .0f));
			positions.push_back(1.0f);
		}
		else if (identifier == "f")
		{
			uint32_t sweep_inds[3];
			for (int i = 0; line != ""; ++i)
			{
				auto face_indices = extract(line, ' ');
				if (face_indices != "")
				{
					sweep_inds[i == 0 ? 0 : 2] = convert(extract(face_indices, '/'), uint32_t(0)) - 1;

					if (i >= 2)
					{
						indices.push_back(sweep_inds[0]);
						indices.push_back(sweep_inds[1]);
						indices.push_back(sweep_inds[2]);
						material_indices.push_back(use_mtl_index);
					}
					sweep_inds[1] = sweep_inds[2];
				}
			}
		}
		else if (identifier == "usemtl")
		{
			use_mtl_index = -1;
			for (auto& mtlname : mtlNames) {
				use_mtl_index++;
				if (mtlname == line)
					break;
			}
		}
	}
}