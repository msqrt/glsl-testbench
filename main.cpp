
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
	glTextureStorage3D(state, 1, GL_RG32F, screenw, screenh, 3);

	LARGE_INTEGER start, frequency;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&start);

	int source = 0;

	int frame = 0;

	while (loop()) // stops if esc pressed or window closed
	{
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		float t = float(double(now.QuadPart-start.QuadPart) / double(frequency.QuadPart));
		
		TimeStamp start;

		if (!simulate)
			simulate = createProgram(
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
				uniform int source;
				uniform int frame;
				uniform float t;
				layout(rg32f) uniform image2DArray state;
				void main() {
					srand(uint(t * 0.) + 1u, uint(gl_GlobalInvocationID.x / 4) + 1u, uint(gl_GlobalInvocationID.y / 4) + 1u);
					srand(uint((t+ rand())) + 1u, uint(gl_GlobalInvocationID.x / 4) + 1u, uint(gl_GlobalInvocationID.y / 4) + 1u);
					vec2 prev = imageLoad(state, ivec3(gl_GlobalInvocationID.xy, source)).xy;
					vec2 diff = -prev*4.;
					if (gl_GlobalInvocationID.x < 1023)
						diff += imageLoad(state, ivec3(gl_GlobalInvocationID.xy + ivec2(1, 0), source)).xy;
					else diff += vec2(4.);
					if (gl_GlobalInvocationID.x >0)
						diff += imageLoad(state, ivec3(gl_GlobalInvocationID.xy+ivec2(-1,0), source)).xy;
					else diff += vec2(4.);
					if (gl_GlobalInvocationID.y < 1023)
						diff += imageLoad(state, ivec3(gl_GlobalInvocationID.xy+ivec2(0,1), source)).xy;
					else diff += vec2(4.);
					if (gl_GlobalInvocationID.y > 0)
						diff += imageLoad(state, ivec3(gl_GlobalInvocationID.xy+ivec2(0,-1), source)).xy;
					else diff += vec2(4.);

					float ext = 1.+.12*cos(10. * sin(float(gl_GlobalInvocationID.x) / 1023. * 3.141592 * 3.) * sin(float(gl_GlobalInvocationID.y) / 1024. * 3.141592 * 3.) + t*2.);
					//ext += .01*sin(float(gl_GlobalInvocationID.x)*.1+ float(gl_GlobalInvocationID.y) * .1) * sin(float(gl_GlobalInvocationID.x) * .1-float(gl_GlobalInvocationID.y)*.1);
					const float s = 1./128.;
					float alpha = 11.9 * (.98 + .04 * rand()), beta = ext*15.4 * (.98 + .04 * rand()); //mix(11., 12., float(gl_GlobalInvocationID.x)/1023.), mix(15., 16., float(gl_GlobalInvocationID.y) / 1023.)
					vec2 vel = 
						vec2(diff.x / 32. + (prev.x * (prev.y - 1.) - alpha) * s,
							diff.y / 8. + (beta - prev.x * prev.y) * s);
					vec2 val = max(vec2(.0), prev + .5*vel + .5*imageLoad(state, ivec3(gl_GlobalInvocationID.xy, 2)).xy);
					//val *= .99;
					if (frame == 0) {
						val = vec2(4.);
						vel = vec2(.0);
					}
					imageStore(state, ivec3(gl_GlobalInvocationID.xy, 1-source), vec4(val, .0, .0));
					imageStore(state, ivec3(gl_GlobalInvocationID.xy, 2), vec4(vel, .0, .0));
				}
				));

		glUseProgram(simulate);
		for (int i = 0; i < 40; ++i) {
			glUniform1f("t", t);
			glUniform1i("frame", frame);
			glUniform1i("source", source);
			bindImage("state", 0, state, GL_READ_WRITE, GL_RG32F);
			glDispatchCompute(64, 64, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			source = 1 - source;
		}

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
					col = vec4(1.-smoothstep(8., 3.7, texelFetch(state, ivec3(gl_FragCoord.xy, layer), 0).y));// pow(texelFetch(state, ivec3(gl_FragCoord.xy, layer), 0).xyyx * vec3(.01, .0015, .0005).zxyx, vec4(.8)) - vec4(.02);
				}
			));

		glUseProgram(draw);
		bindTexture("state", state);
		glUniform1i("layer", source);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		TimeStamp end;

		font.drawText(L"⏱: " + std::to_wstring(end-start), 10.f, 10.f, 15.f); // wstring, x, y, font size
		swapBuffers();
		frame++;
	}
	return 0;
}
