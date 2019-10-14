
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

#include <filesystem>

#include "inline_glsl.h"

const int screenw = 1280, screenh = 720;

void loadObject();
std::vector<float> positions;
std::vector<uint32_t> indices;
std::vector<uint32_t> material_indices;
std::vector<float> material_albedo;

void swap_if_ok(Program& old, const GLuint&& replacement) {
	if (replacement) old = replacement;
}

int main() {

	OpenGL context(screenw, screenh, "example");
	Font font(L"Consolas");

	dummyCompile(GLSL(460, void main() {}));

	Program objRender, blit;

	loadObject();

	Buffer positionBuffer, indexBuffer, materialIndexBuffer, materialAlbedoBuffer;
	glNamedBufferStorage(positionBuffer, sizeof(float)*positions.size(), positions.data(), 0);
	glNamedBufferStorage(indexBuffer, sizeof(float)*indices.size(), indices.data(), 0);
	glNamedBufferStorage(materialIndexBuffer, sizeof(float)*material_indices.size(), material_indices.data(), 0);
	glNamedBufferStorage(materialAlbedoBuffer, sizeof(float)*material_albedo.size(), material_albedo.data(), 0);

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

	auto previous = std::filesystem::last_write_time(std::filesystem::path(__FILE__));

	while (loop()) // stops if esc pressed or window closed
	{
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		float t = float(double(now.QuadPart-start.QuadPart) / double(frequency.QuadPart));

		std::error_code ec;
		auto fileModify = std::filesystem::last_write_time(std::filesystem::path(__FILE__), ec);
		const bool reloadShaders = !ec && fileModify > previous;
		if (ec) {
			std::cout << ec.message() << std::endl;
		}
		if (reloadShaders) previous = fileModify;
		TimeStamp start;

		if (!objRender || reloadShaders)
			swap_if_ok(objRender,
				createProgram(
		GLSL(460,
			layout(std430) buffer; // set as default

			buffer positionBuffer{ vec4 position[]; };

			uniform mat4 camToClip;
			uniform float t;

			out vec3 world_position;

			void main() {
				vec4 p = position[gl_VertexID];
				p.xyz *= .0008;
				world_position = p.xyz;
				p.z += .12;
				p.x -= .4;
				const float c = cos(t*.5), s = sin(t*.5);
				p.xz = mat2(c, s, -s, c) * p.xz;
				p.y -= .3;
				p.z -= .5;

				gl_Position = camToClip * p;
			}
		), "", "",
		GLSL(460,
			layout(std430) buffer;

			buffer materialIndexBuffer{ uint materialIndex[]; };
			buffer materialAlbedoBuffer{ vec4 materialAlbedo[]; };

			layout(triangles) in;
			layout(triangle_strip, max_vertices = 3) out;

			in vec3 world_position[];
			out vec3 albedo, position, normal_varying;

			uniform int res;
			uniform float t;

			void main() {
				vec4 pos[3];
				for (int i = 0; i < 3; ++i)
					pos[i] = gl_in[i].gl_Position;

				albedo = materialAlbedo[materialIndex[gl_PrimitiveIDIn]].xyz; // fetch albedo for triangle

				vec3 normal = normalize(cross(world_position[1].xyz - world_position[0].xyz, world_position[2].xyz - world_position[0].xyz));

				vec3 camera_location = vec3(.0, .3, .5);
				const float c = cos(t*.5), s = sin(t*.5);
				camera_location.xz = mat2(c, -s, s, c) * camera_location.xz;
				camera_location += vec3(.4, .0, -.12);

				if (dot(camera_location-world_position[0], normal) < .0) normal = -normal;

				for (int i = 0; i < 3; ++i) {
					normal_varying = normal;
					position = world_position[i];
					gl_Position = pos[i];
					EmitVertex();
				}
				EndPrimitive();
			}
		), GLSL(460,
			in vec3 albedo, position, normal_varying;
			out vec3 color, normal;

			uniform sampler2D logo;

			void main() {
				color = vec3(1.) - texture(logo, vec2(.0, 1.) + vec2(1., -1.)*clamp(position.xy*vec2(-2., 2.) - vec2(-1.0, .1), vec2(.0), vec2(1.))).xyz;
				if (position.z < .46) color = vec3(1.);
				color *= albedo;
				normal = normal_varying;
			}
		)));

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

		if (!blit || reloadShaders)
			swap_if_ok(
				blit,
				createProgram(
				GLSL(460,
					out vec2 uv;
					void main() {
						uv = vec2(gl_VertexID == 1 ? 4. : -1., gl_VertexID == 2 ? 4. : -1.);
						gl_Position = vec4(uv, -.5, 1.);
					}
				)
				, "", "", "",
				GLSL(460,

					uvec4 rndseed;
					void jenkins_mix()
					{
						rndseed.x -= rndseed.y; rndseed.x -= rndseed.z; rndseed.x ^= rndseed.z >> 13;
						rndseed.y -= rndseed.z; rndseed.y -= rndseed.x; rndseed.y ^= rndseed.x << 8;
						rndseed.z -= rndseed.x; rndseed.z -= rndseed.y; rndseed.z ^= rndseed.y >> 13;
						rndseed.x -= rndseed.y; rndseed.x -= rndseed.z; rndseed.x ^= rndseed.z >> 12;
						rndseed.y -= rndseed.z; rndseed.y -= rndseed.x; rndseed.y ^= rndseed.x << 16;
						rndseed.z -= rndseed.x; rndseed.z -= rndseed.y; rndseed.z ^= rndseed.y >> 5;
						rndseed.x -= rndseed.y; rndseed.x -= rndseed.z; rndseed.x ^= rndseed.z >> 3;
						rndseed.y -= rndseed.z; rndseed.y -= rndseed.x; rndseed.y ^= rndseed.x << 10;
						rndseed.z -= rndseed.x; rndseed.z -= rndseed.y; rndseed.z ^= rndseed.y >> 15;
					}
					void srand(uint A, uint B, uint C) { rndseed = uvec4(A, B, C, 0); jenkins_mix(); }

					float rand()
					{
						if (0 == rndseed.w++ % 3) jenkins_mix();
						rndseed.xyz = rndseed.yzx;
						return float(rndseed.x) / pow(2., 32.);
					}

					uniform sampler2D albedo, normal, depth;
					uniform float t;
					in vec2 uv;
					out vec3 color;

					void main() {
						srand(floatBitsToUint(t), uint(gl_FragCoord.x)+1u, uint(gl_FragCoord.y)+1u);
						vec2 coord = uv * .5 + vec2(.5);
						vec3 n = texture(normal, coord).xyz;
						color = texture(albedo,coord).xyz*max(.1, dot(n, normalize(vec3(1.))));
						float curDepth = texture(depth, coord).x;
						float ang = rand()*2.*3.141592;
						float off = (1. + sqrt(5.)) * 3.1415926535;
						int count = 0;
						for (int i = 0; i < 16; ++i) {
							ang += off;
							vec2 offset = .05*float(i) / 15.*vec2(cos(ang), sin(ang));
							if (texture(depth, coord + offset).x > curDepth-.001) count++;
						}
						color *= float(count) / 15.;
						//color = mix(color, vec3(.7, .7, .9), abs(-.005 / abs(texture(depth, coord).x - 1.)));
					}
					)
				));
		glUseProgram(blit);
		bindTexture("albedo", albedo);
		bindTexture("normal", normal);
		bindTexture("depth", depth);
		glUniform1f("t", t);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		TimeStamp end;

		font.drawText(L"⏱: " + std::to_wstring(end-start), 10.f, 10.f, 15.f); // wstring, x, y, font size
		swapBuffers();
	}
	return 0;
}

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
			if (mtlNames.size()>0)
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
					sweep_inds[i==0?0:2] = convert(extract(face_indices, '/'), uint32_t(0)) - 1;
					
					if(i>=2)
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