
#include "window.h"
#include "shaderprintf.h"
#include "gl_helpers.h"
#include "math_helpers.h"
#include "gl_timing.h"
#include "math.hpp"
#include "text_renderer.h"
#include "inline_glsl.h"

#include <vector>
#include <random>
#include <fstream>
#include <string>
#include <string_view>
#include <charconv>

const int screenw = 1024, screenh = 1024;

int main() {
	OpenGL context(screenw, screenh, "example");
	Font font(L"Consolas");

	Program simulate, draw;

	Texture<GL_TEXTURE_2D_ARRAY> state;
	glTextureStorage3D(state, 1, GL_RG32F, screenw, screenh, 2);

	LARGE_INTEGER start, frequency;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&start);

	int source = 0;

	while (loop()) // stops if esc pressed or window closed
	{
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		float t = float(double(now.QuadPart-start.QuadPart) / double(frequency.QuadPart));
		
		TimeStamp start;

		if (!simulate)
			simulate = createProgram(
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
				void srand(uint A, uint B, uint C) { rndseed = uvec4(A, B, C, 0); jenkins_mix(); jenkins_mix(); }

				float rand()
				{
					if (0 == rndseed.w++ % 3) jenkins_mix();
					rndseed.xyz = rndseed.yzx;
					return float(rndseed.x) / pow(2., 32.);
				}
				layout(local_size_x = 16, local_size_y = 16) in;
				uniform int source;
				uniform float t;
				layout(rg32f) uniform image2DArray state;
				void main() {
					srand(uint(t*10.)+1u, uint(gl_GlobalInvocationID.x/4) + 1u, uint(gl_GlobalInvocationID.y/4) + 1u);
					vec2 prev = imageLoad(state, ivec3(gl_GlobalInvocationID.xy, source)).xy;
					vec2 diff = -prev*4.;
					if (gl_GlobalInvocationID.x < 1023)
						diff += imageLoad(state, ivec3(gl_GlobalInvocationID.xy+ivec2(1,0), source)).xy;
					if (gl_GlobalInvocationID.x >0)
						diff += imageLoad(state, ivec3(gl_GlobalInvocationID.xy+ivec2(-1,0), source)).xy;
					if (gl_GlobalInvocationID.y < 1023)
						diff += imageLoad(state, ivec3(gl_GlobalInvocationID.xy+ivec2(0,1), source)).xy;
					if (gl_GlobalInvocationID.y > 0)
						diff += imageLoad(state, ivec3(gl_GlobalInvocationID.xy+ivec2(0,-1), source)).xy;

					float ext = .005+.15*sin(float(gl_GlobalInvocationID.x) / 1023.*8.+t*.5)*cos(float(gl_GlobalInvocationID.y)*.01);

					vec2 val = max(vec2(.0), prev + .01 *
						vec2(diff.x*.5+prev.x - pow(prev.x, 3.) - prev.y*.6 + ext,
							(diff.y*2.+prev.x - prev.y)*.1));
					//val *= .99;
					imageStore(state, ivec3(gl_GlobalInvocationID.xy, 1-source), vec4(val, .0, .0));
				}
				));

		glUseProgram(simulate);
		glUniform1f("t", t);
		glUniform1i("source", source);
		bindImage("state", 0, state, GL_READ_WRITE, GL_RG32F);
		glDispatchCompute(64, 64, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		
		source = 1 - source;

		if (!draw)
			draw = createProgram(
				GLSL(460,
				void main() {
					gl_Position = vec4(gl_VertexID == 1 ? 4. : -1., gl_VertexID == 2 ? 4. : -1., -.5, 1.);
				}
		),
				"", "", "",
			GLSL(460,
				uniform sampler2DArray state;
				out vec4 col;
				uniform int layer;
				void main() {
					col = texelFetch(state, ivec3(gl_FragCoord.xy, layer), 0).xxxx*.4;
				}
			));

		glUseProgram(draw);
		bindTexture("state", state);
		glUniform1i("layer", source);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		TimeStamp end;

		font.drawText(L"⏱: " + std::to_wstring(end-start), 10.f, 10.f, 15.f); // wstring, x, y, font size
		swapBuffers();

	}
	return 0;
}
