
#include "testbench.h"

// this is a small example for 'glsl testbench', a small utility library I've been working on lately.
// by compiling and running, you should see some pretty patterns appear -- this file implements a
// crude reaction-diffusion simulator that runs on the GPU with relatively little hassle.

// the two key features of the testbench are easy binding (everything is bound with the shader name)
// and a GLSL source macro 2.0 that allows you to edit the shader code while the executable is running
// try running the code and commenting out some of the different color schemes in the second shader
// around line 170! when you save this file, the output of the executable should change correspondingly.
// (no magic; it just stores the location of the shader source in the cpp file and re-reads it on file update)

// there's also a bunch of other convenience features, and pretty much everything is modular so that you can
// utilize even small parts in your own framework (the timing system, for example)

const int screenw = 1024, screenh = 1024;

int main() {

	// create window and context. can also do fullscreen and non-visible window for compute only
	OpenGL context(screenw, screenh, "example");
	
	// load a font to draw text with -- any system font or local .ttf file should work
	Font font(L"Consolas");

	// shader variables; could also initialize them here, but it's often a good idea to
	// do that at the callsite (so input/output declarations are close to the bind code)
	Program simulate, draw;

	// this variable simply makes a scope-safe GL object so glCreateTexture is called here
	// and glDeleteTexture on scope exit; different types exist. gl_helpers.h contains more
	// helpers like this for buffers, renderbuffers and framebuffers.
	Texture<GL_TEXTURE_2D_ARRAY> state;
	// two color channels; the reaction we're modeling has two components that react to each
	// other, each channel will represent the concentration of that component at each pixel
	// array length 3; we wish to store 2 states and a velocity
	glTextureStorage3D(state, 1, GL_RG32F, screenw, screenh, 3);

	// we're rendering from one image to the other and then back in a 'ping-pong' fashion; source keeps track of 
	// which image is the source and which is the target.
	int source_target = 0, frame = 0;

	while (loop()) // loop() stops if esc pressed or window closed
	{
		
		// timestamp objects make gl queries at those locations; you can substract them to get the time
		TimeStamp start;

		if (!simulate)
			// single-argument createProgram() makes a compute shader.
			// the argument can be either a filepath or the source directly as given by the GLSL macro:
			simulate = createProgram(
				GLSL(460,
				// the local thread block size; the program will be ran in sets of 16 by 16 threads.
				layout(local_size_x = 16, local_size_y = 16) in;
				
				// simple pseudo-RNG based on the jenkins hash mix function
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
					return float((rndseed.xyz = rndseed.yzx).x) / pow(2., 32.);
				}

				// uniform variables are global from the glsl perspective; you set them in the CPU side and every thread gets the same value
				uniform int source;
				uniform int frame;
				// images are also uniforms; we also need to declare the type of the image, here it's two channels of 32-bit floating point numbers
				layout(rg32f) uniform image2DArray state;

				void main() {
					// seed with seeds that change at different time offsets (not crucial to the algorithm but yields nicer results)
					srand(1u, uint(gl_GlobalInvocationID.x / 4) + 1u, uint(gl_GlobalInvocationID.y / 4) + 1u);
					srand(uint((rand())) + uint(frame/100), uint(gl_GlobalInvocationID.x / 4) + 1u, uint(gl_GlobalInvocationID.y / 4) + 1u);

					// apply laplacian stencil for diffusion; this is effectively a blur on the input images
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

					// how much we're adding component x to the frame (this could be any black and white image or animation!)
					float ext = 1.+.12*cos(10. * sin(float(gl_GlobalInvocationID.x) / 1023. * 3.141592 * 3.) * sin(float(gl_GlobalInvocationID.y) / 1024. * 3.141592 * 3.) + float(frame)*.015);
					
					// some simulation parameters
					const float s = 1./128.;
					float alpha = 11.9 * (.98 + .04 * rand()), beta = ext*15.4 * (.98 + .04 * rand());
					// try uncommenting these to (pretty much) recreate figure 2 from https://www.researchgate.net/publication/220494187_Advanced_Reaction-Diffusion_Models_for_Texture_Synthesis
					// also see the related color scheme in the draw shader!
					//alpha = mix(8., 20., float(gl_GlobalInvocationID.x) / 1023.) * (.99 + .02 * rand()); beta = mix(8., 20., float(gl_GlobalInvocationID.y) / 1023.) * (.99 + .02 * rand());
					// compute reaction velocity; how the concentrations of the components change
					vec2 vel = 
						vec2(diff.x / 32. + (prev.x * (prev.y - 1.) - alpha) * s,
							diff.y / 8. + (beta - prev.x * prev.y) * s);
					// leapfrog integration step improves stability; we move half of the timestep with the old velocity and half with the new one
					vec2 val = max(vec2(.0), prev + .5*vel + .5*imageLoad(state, ivec3(gl_GlobalInvocationID.xy, 2)).xy);
					
					// on first frame we simply init to some values
					if (frame == 0) {
						val = vec2(4.);
						vel = vec2(.0);
					}
					// store results; imageStore always wants vec4s as input
					imageStore(state, ivec3(gl_GlobalInvocationID.xy, 1-source), vec4(val, .0, .0));
					imageStore(state, ivec3(gl_GlobalInvocationID.xy, 2), vec4(vel, .0, .0));
				}
				));

		// use the program and render a bunch of times
		glUseProgram(simulate);
		for (int i = 0; i < 40; ++i) {
			// for convenience, uniforms can be bound directly with names
			glUniform1i("frame", frame);
			glUniform1i("source", source_target);
			// .. this includes images
			bindImage("state", 0, state, GL_READ_WRITE, GL_RG32F);
			// the arguments of dispatch are the numbers of thread blocks in each direction;
			// since our local size is 16x16x1, we'll get 1024x1024x1 threads total, just enough
			// for our image
			glDispatchCompute(64, 64, 1);

			// we're writing to an image in a shader, so we should have a barrier to ensure the writes finish
			// before the next shader call (wasn't an issue on my hardware in this case, but you should always make sure
			// to place the correct barriers when writing from compute shaders and reading in subsequent shaders)
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// flip the ping-pongt flag
			source_target = 1 - source_target;
		}

		if (!draw)
			// the graphics program version of createProgram() takes 5 sources; vertex, control, evaluation, geometry, fragment
			draw = createProgram(
				GLSL(460,
					void main() {
						// note that nobody forces you to build VAOs and VBOs; you can write
						// arbitrary vertex shaders as long as they output gl_Position
						gl_Position = vec4(gl_VertexID == 1 ? 4. : -1., gl_VertexID == 2 ? 4. : -1., -.5, 1.);
					}
				),
				"", "", "",
				GLSL(460,
					uniform sampler2DArray state;
					out vec4 col;
					uniform int layer;
					void main() {
						col = vec4(1.-smoothstep(8., 3.7, texelFetch(state, ivec3(gl_FragCoord.xy, layer), 0).y));
						// other color schemes:
						//col = pow(texelFetch(state, ivec3(gl_FragCoord.xy, layer), 0).xyyx * vec3(.01, .0015, .002).zxyx, vec4(.8)) - vec4(.02);
						//col = vec4(texelFetch(state, ivec3(gl_FragCoord.xy, layer), 0).x*.005); // this rooughly matches figures from the aforementioned article
					}
				)
			);

		glUseProgram(draw);
		// textures are also a oneliner to bind (same for buffers; no bindings have to be states in the shader!)
		// note that this relies on the shader loader setting their values beforehand; if you use your own shader
		// loader, you'll have to replicate this (see how assignUnits() is used in program.cpp)
		bindTexture("state", state);
		glUniform1i("layer", source_target);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		// here we're just using two timestamps, but you could of course measure intermediate timings as well
		TimeStamp end;

		// print the timing (word of warning; this forces a cpu-gpu synchronization)
		font.drawText(L"⏱: " + std::to_wstring(end-start), 10.f, 10.f, 15.f); // text, x, y, font size

		// this actually displays the rendered image
		swapBuffers();
		frame++;
	}
	return 0;
}
