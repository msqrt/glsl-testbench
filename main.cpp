
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
std::vector<float> positions, normals, texcoords, material_albedo;
std::vector<uint32_t> position_indices, normal_indices, texcoord_indices, material_indices;

int main() {

	OpenGL context(screenw, screenh, "example");
	Font font(L"Consolas");

	Program objRender, subSurface, aTrous, blit;

	loadObject();

	auto makeBuffer = [](auto data) { Buffer b; glNamedBufferStorage(b, data.size() * 4, data.data(), 0); return b; };

	Buffer positionBuffer = makeBuffer(positions);
	Buffer normalBuffer = makeBuffer(normals);
	Buffer texcoordBuffer = makeBuffer(texcoords);
	
	Buffer posIndexBuffer = makeBuffer(position_indices);
	Buffer norIndexBuffer = makeBuffer(normal_indices);
	Buffer tcIndexBuffer = makeBuffer(texcoord_indices);
	
	Buffer materialIndexBuffer = makeBuffer(material_indices);
	Buffer materialAlbedoBuffer = makeBuffer(material_albedo);

	Texture<GL_TEXTURE_2D> screenAlbedo, screenNormal, screenDepth;
	Texture<GL_TEXTURE_2D_ARRAY> screenColor;
	glTextureStorage2D(screenAlbedo, 1, GL_RGB16F, screenw, screenh);
	glTextureStorage2D(screenNormal, 1, GL_RGB16F, screenw, screenh);
	glTextureStorage3D(screenColor, 1, GL_RGBA16F, screenw, screenh, 2);
	glTextureStorage2D(screenDepth, 1, GL_DEPTH_COMPONENT32F, screenw, screenh);
	
	Texture<GL_TEXTURE_2D_ARRAY> lightAlbedo, lightNormal, lightDepth;
	constexpr int views = 3, shadowRes = 2048;
	glTextureStorage3D(lightAlbedo, 1, GL_RGB16F, shadowRes, shadowRes, views);
	glTextureStorage3D(lightNormal, 1, GL_RGB16F, shadowRes, shadowRes, views);
	glTextureStorage3D(lightDepth, 1, GL_DEPTH_COMPONENT32F, shadowRes, shadowRes, views);

	auto loadWithFilters = [](auto path) {
		auto tex = loadImage(path);
		glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameterf(tex, GL_TEXTURE_MAX_ANISOTROPY, 1024.f);
		return tex;
	};

	auto albedo = loadWithFilters("D:/3d/lambertian.png");
	auto normals = loadWithFilters("D:/3d/normal.png");

	std::array<float, 16*(1+views)> camToClip;
	setupProjection(camToClip.data(), .5f, screenw / float(screenh), .01f, 4.f);
	for (int i = 0; i < views; ++i)
		setupOrtho(&camToClip[16 * (i + 1)], 1.f, .35f, .01f, 4.f);

	Buffer camToClips = makeBuffer(camToClip), camToWorlds;

	LARGE_INTEGER start, frequency;
	QueryPerformanceCounter(&start);
	QueryPerformanceFrequency(&frequency);

	while (loop()) // stops if esc pressed or window closed
	{
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		float t = float(double(now.QuadPart-start.QuadPart) / double(frequency.QuadPart));

		std::array<float, 16 * (1 + views)> camToWorld;
		lookAt(camToWorld.data(), cos(t*.2f), -.3f, sin(t*.2f)-.1f, .0f, -.1f, -.1f);
		for (int i = 0; i < views; ++i) {
			const float y = 1.f;// i % 2 ? 1.f : -1.f;
			lookAt(&camToWorld[16 * (i + 1)], .8f*cos(float(i)*3.141592f*.5f-t*.3f), .4f*y, .8f*sin(float(i)*3.141592f*.5f-t*.3f) - .1f, .0f, -.2f*y, -.1f);
		}
		glNamedBufferData(camToWorlds, sizeof(float)*camToWorld.size(), camToWorld.data(), GL_STREAM_DRAW);

		TimeStamp start;

		if (!objRender)
			objRender = createProgram(
				GLSL(460,
					layout(std430) buffer; // set as default

			buffer positionBuffer{ vec4 position[]; };

			uniform camToWorldBuffer{ mat4 camToWorld[4]; };
			uniform camToClipBuffer{ mat4 camToClip[4]; };
			uniform float t;
			uniform int matrixOffset;

			out vec3 world_position;
			out int instanceID;

			void main() {
				vec4 p = position[gl_VertexID];
				world_position = p.xyz;
				instanceID = gl_InstanceID;
				gl_Position = camToClip[instanceID+matrixOffset] * inverse(camToWorld[instanceID+matrixOffset]) * p;
			}
			), "", "",
				GLSL(460,
					layout(std430) buffer;

			buffer normalBuffer{ vec4 normal[]; };
			buffer texcoordBuffer{ vec2 texcoord[]; };
			buffer normalIndexBuffer{ uint normalIndex[]; };
			buffer texcoordIndexBuffer{ uint texcoordIndex[]; };
			buffer materialIndexBuffer{ uint materialIndex[]; };
			buffer materialAlbedoBuffer{ vec4 materialAlbedo[]; };

			layout(triangles) in;
			layout(triangle_strip, max_vertices = 3) out;

			in vec3 world_position[];
			in int instanceID[];
			out vec3 albedo, position, normal_varying;
			out vec2 texcoord_varying;

			uniform camToWorldBuffer{ mat4 camToWorld[4]; };

			uniform float t;
			uniform int matrixOffset;

			void main() {
				vec4 pos[3];
				for (int i = 0; i < 3; ++i)
					pos[i] = gl_in[i].gl_Position;

				albedo = materialAlbedo[materialIndex[gl_PrimitiveIDIn]].xyz; // fetch albedo for triangle

				vec3 camera_location = camToWorld[instanceID[0]+matrixOffset][0].xyz;
				gl_Layer = instanceID[0];

				for (int i = 0; i < 3; ++i) {
					normal_varying = normal[normalIndex[gl_PrimitiveIDIn * 3 + i]].xyz;
					//if (dot(camera_location - world_position[0], normal_varying) < .0) normal_varying = -normal_varying;
					texcoord_varying = texcoord[texcoordIndex[gl_PrimitiveIDIn * 3 + i]].xy;
					position = world_position[i];
					gl_Position = pos[i];
					EmitVertex();
				}
				EndPrimitive();
			}
			),
				GLSL(460,
			in vec3 albedo, position, normal_varying;
			in vec2 texcoord_varying;
			out vec3 color, normal;

			uniform sampler2D albedomap, normalmap;

			void main() {
				color = texture(albedomap, texcoord_varying).xyz;
				normal = normalize(texture(normalmap, texcoord_varying).xyz-vec3(.5));
			}
			)
				);

		glUseProgram(objRender);

		// object binds
		bindBuffer("positionBuffer", positionBuffer);
		bindBuffer("normalBuffer", normalBuffer);
		bindBuffer("texcoordBuffer", texcoordBuffer);
		bindBuffer("normalIndexBuffer", norIndexBuffer);
		bindBuffer("texcoordIndexBuffer", tcIndexBuffer);
		bindBuffer("materialIndexBuffer", materialIndexBuffer);
		bindBuffer("materialAlbedoBuffer", materialAlbedoBuffer);

		bindTexture("albedomap", albedo);
		bindTexture("normalmap", normals);
		bindBuffer("camToClipBuffer", camToClips);
		bindBuffer("camToWorldBuffer", camToWorlds);
		glUniform1i("matrixOffset", 0);
		glUniform1f("t", t);
		
		// multiple rendertargets
		Framebuffer fbo;
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		bindOutputTexture("color", screenAlbedo);
		bindOutputTexture("normal", screenNormal);
		bindOutputDepthTexture(screenDepth);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// state changes (classic opengl)
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, posIndexBuffer);
		glEnable(GL_DEPTH_TEST);

		glDrawElements(GL_TRIANGLES, (GLsizei)position_indices.size(), GL_UNSIGNED_INT, nullptr);

		float one = 1.f;
		float zeros[] = { .0f, .0f, .0f };
		glClearTexSubImage(lightAlbedo, 0, 0, 0, 0, shadowRes, shadowRes, views, GL_RGB, GL_FLOAT, zeros);
		glClearTexSubImage(lightNormal, 0, 0, 0, 0, shadowRes, shadowRes, views, GL_RGB, GL_FLOAT, zeros);
		glClearTexSubImage(lightDepth, 0, 0, 0, 0, shadowRes, shadowRes, views, GL_DEPTH_COMPONENT, GL_FLOAT, &one);

		Framebuffer lightFbo;
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		bindOutputTexture("color", lightAlbedo);
		bindOutputTexture("normal", lightNormal);
		bindOutputDepthTexture(lightDepth);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		glUniform1i("matrixOffset", 1);

		glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)position_indices.size(), GL_UNSIGNED_INT, nullptr, views);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glDisable(GL_DEPTH_TEST);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, screenw, screenh); // viewport has to be set by hand :/

		if (!subSurface)
			subSurface = createProgram(
			GLSL(460,

				layout(local_size_x = 16, local_size_y = 16) in;
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
				void srand(uint A, uint B, uint C) { rndseed = uvec4(A, B, C, 0); jenkins_mix(); jenkins_mix(); }

				float rand()
				{
					if (0 == rndseed.w++ % 3) jenkins_mix();
					rndseed.xyz = rndseed.yzx;
					return float(rndseed.x) / pow(2., 32.);
				}
				uniform camToWorldBuffer{ mat4 camToWorld[4]; };
				uniform camToClipBuffer{ mat4 camToClip[4]; };

				uniform sampler2D albedo, normal, depth;
				uniform sampler2DArray lightAlbedo, lightNormal, lightDepth;
				uniform float t;
				layout(rgba16f) uniform writeonly image2DArray result;

				void main() {
					srand(floatBitsToUint(t), uint(gl_GlobalInvocationID.x) + 1u, uint(gl_GlobalInvocationID.y) + 1u);
					const ivec3 resolution = imageSize(result);
					if (gl_GlobalInvocationID.x < resolution.x && gl_GlobalInvocationID.y < resolution.y) {
						const vec2 uv = vec2(gl_GlobalInvocationID.xy) / vec2(resolution)*2.-vec2(1.);
						const vec2 coord = uv * .5 + vec2(.5);
						const vec3 n = texture(normal, coord).xyz;
						vec4 p = camToWorld[0] * inverse(camToClip[0])* vec4(uv, texture(depth, coord).x*2. - 1., 1.);
						p /= p.w;
						vec3 color = vec3(.0);
						const float phi = (1. + sqrt(5.)) * 3.141952;
						vec3 view = normalize(camToWorld[0][3].xyz - p.xyz);
						for (int i = 0; i < 3; i++) {
							vec4 p_shadow = camToClip[i + 1] * inverse(camToWorld[i + 1]) * p;
							p_shadow.xyz /= p_shadow.w;
							const vec2 shadow_coord = p_shadow.xy*.5 + vec2(.5);
							float ang = rand()*2.*3.141592;
							const float off = rand();
							const int N = 8;
							vec3 lcol = .8*vec3(1., 1., 1.);
							if (i == 0) lcol = .01*vec3(.5, .5, .6);
							else if (i == 2) lcol = .01*vec3(.7, .8, .7);
							//if (dot(n, camToWorld[i + 1][2].xyz) > .0 && texture(lightDepth, vec3(p_shadow.xy*.5 + vec2(.5), float(i))).x > p_shadow.z*.5 + .495)
							//	color += lcol * vec3(4.)*pow(max(.0, dot(n, normalize(view + camToWorld[i + 1][2].xyz))), 80.);
							if(false)
								color += lcol * vec3(1.,.5,.5)*max(.0, dot(n, camToWorld[i + 1][2].xyz));
							else
							for (int j = 0; j < N; ++j) {
								ang += phi;
								vec2 p_pt = p_shadow.xy + .01*vec2(cos(ang), sin(ang))*sqrt((float(j) + off) / float(N));
								vec4 p_new = camToWorld[i + 1] * inverse(camToClip[i + 1])* vec4(p_pt, texture(lightDepth, vec3(p_pt*.5 + vec2(.5), float(i))).x*2. - 1., 1.);
								p_new.xyz /= p_new.w;
								vec3 n_new = texture(lightNormal, vec3(p_pt*.5 + vec2(.5), float(i))).xyz;
								const vec3 diff = p.xyz - p_new.xyz;
								vec3 diffusion = exp(-2.*vec3(250., 350., 370.)*length(diff));
								float input_lambert = max(.0, dot(camToWorld[i + 1][2].xyz, n_new));
								float output_cone = max(.0, dot(-normalize(diff) + .5*camToWorld[i + 1][2].xyz, camToWorld[i + 1][2].xyz));

								color += 10. * lcol *input_lambert*diffusion*output_cone / float(N);
							}
						}
						imageStore(result, ivec3(gl_GlobalInvocationID.xy, 0), vec4(color, 1.));
					}
				}
			)
		);
		glUseProgram(subSurface);
		bindTexture("albedo", screenAlbedo);
		bindTexture("normal", screenNormal);
		bindTexture("depth", screenDepth);
		bindTexture("lightAlbedo", lightAlbedo);
		bindTexture("lightNormal", lightNormal);
		bindTexture("lightDepth", lightDepth);
		bindBuffer("camToClipBuffer", camToClips);
		bindBuffer("camToWorldBuffer", camToWorlds);
		glUniform1f("t", t);
		bindImage("result", 0, screenColor, GL_WRITE_ONLY, GL_RGBA16F);
		glDispatchCompute((screenw+15)/16, (screenh+15)/16, 1);

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		if (!aTrous)
			aTrous = createProgram(GLSL(460,
				uniform sampler2D normal, depth;
				layout(local_size_x = 16, local_size_y = 16) in;
				layout(rgba16f) uniform image2DArray result;
				uniform int iter;
				void main() {
					const ivec3 resolution = imageSize(result);
					if (gl_GlobalInvocationID.x < resolution.x && gl_GlobalInvocationID.y < resolution.y) {
						const int in_layer = iter & 1;
						const int out_layer = 1 - in_layer;

						const float p_depth = texelFetch(depth, ivec2(gl_GlobalInvocationID.xy), 0).x;
						const vec3 p_n = texelFetch(normal, ivec2(gl_GlobalInvocationID.xy), 0).xyz;
						const vec3 p_rt = imageLoad(result, ivec3(gl_GlobalInvocationID.xy, in_layer)).xyz;
						const int off = 1 << iter;

						vec4 res = vec4(p_rt, 1.);
						for(int i = int(gl_GlobalInvocationID.x)-2*off; i<= int(gl_GlobalInvocationID.x) + 2*off; i += off)
						for(int j = int(gl_GlobalInvocationID.y)-2*off; j<= int(gl_GlobalInvocationID.y) + 2*off; j += off)
							if (i >= 0 && j >= 0 && i < resolution.x && j < resolution.y) {
								if (ivec2(i, j) == ivec2(gl_GlobalInvocationID.xy))
									continue;

								const float c_depth = texelFetch(depth, ivec2(i, j), 0).x;
								const vec3 c_n = texelFetch(normal, ivec2(i, j), 0).xyz;
								const vec3 c_rt = imageLoad(result, ivec3(i, j, in_layer)).xyz;
								const vec3 diff = c_rt - p_rt;
								const float w = exp(-.3*length(vec2(i,j)-vec2(gl_GlobalInvocationID.xy)))
									*max(.0, dot(c_n, p_n))
									* exp(-2.e4*abs(c_depth-p_depth))
									* exp(-.05*length(diff));
								if(!isnan(w))
									res += w*vec4(c_rt, 1.);
							}
						

						imageStore(result, ivec3(gl_GlobalInvocationID.xy, out_layer), vec4(res.xyz / res.w, 1.));
					}
				}
				));

		glUseProgram(aTrous);
		constexpr int iters = 2;
		for (int i = 0; i < iters; ++i) {
			bindTexture("normal", screenNormal);
			bindTexture("depth", screenDepth);

			bindImage("result", 0, screenColor, GL_READ_WRITE, GL_RGBA16F);
			glUniform1i("iter", i);

			glDispatchCompute((screenw + 15) / 16, (screenh + 15) / 16, 1);

			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		}

		if (!blit)
			blit = createProgram(
				GLSL(460, out vec2 uv; void main() { uv = vec2((gl_VertexID == 1) ? 4. : -1., (gl_VertexID == 2) ? 4. : -1.); gl_Position = vec4(uv, -.5, 1.); }), "", "", "",
				GLSL(460, in vec2 uv; uniform sampler2DArray result; uniform sampler2D albedo; uniform int layer; out vec3 color; void main() { color = texture(result, vec3(uv*.5 + vec2(.5), float(layer))).xyz*texture(albedo, uv*.5+vec2(.5)).xyz; color /= .2 + length(color); }));
		glUseProgram(blit);
		
		bindTexture("result", screenColor);
		bindTexture("albedo", screenAlbedo);
		glUniform1i("layer", iters & 1);

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
	string mtlfile(istreambuf_iterator<char>(ifstream("D:/3d/head.mtl").rdbuf()), istreambuf_iterator<char>());
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

	string file(istreambuf_iterator<char>(ifstream("D:/3d/head.OBJ").rdbuf()), istreambuf_iterator<char>());
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
		else if (identifier == "vn")
		{
			for (int i = 0; i < 3; ++i)
				normals.push_back(convert(extract(line, ' '), .0f));
			normals.push_back(.0f);
		}
		else if (identifier == "vt")
		{
			for (int i = 0; i < 2; ++i)
				texcoords.push_back(convert(extract(line, ' '), .0f));
		}
		else if (identifier == "f")
		{
			uint32_t sweep_inds[3][3];
			for (int i = 0; line != ""; ++i)
			{
				auto face_indices = extract(line, ' ');
				if (face_indices != "")
				{
					for(int j = 0; j<3; ++j)
						sweep_inds[j][i==0?0:2] = convert(extract(face_indices, '/'), uint32_t(0)) - 1;
					
					if(i>=2)
					{
						for (int j = 0; j < 3; ++j) {
							position_indices.push_back(sweep_inds[0][j]);
							normal_indices.push_back(sweep_inds[1][j]);
							texcoord_indices.push_back(sweep_inds[2][j]);
						}
						material_indices.push_back(use_mtl_index);
					}
					for(int j = 0; j<3; ++j)
						sweep_inds[j][1] = sweep_inds[j][2];
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