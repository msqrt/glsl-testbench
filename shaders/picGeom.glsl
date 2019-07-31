
#version 450

layout(points) in;
layout(triangle_strip, max_vertices=4) out;

layout(std430) buffer posBuffer {vec4 particle_pos[];};
layout(std430) buffer velBuffer {vec4 particle_vel[];};

out vec2 uv;
out vec3 ps;
uniform mat4 worldToCam, camToClip;

void main() {
	
	vec4 pos = vec4(particle_pos[gl_PrimitiveIDIn].xyz, 1.);
	vec4 velocity = particle_vel[gl_PrimitiveIDIn];

	vec4 cpos = worldToCam * pos;
	cpos /= cpos.w; // probs unnecessary

	for(int i = 0; i<4; ++i) {
		uv = vec2(float(i%2), float(i/2));
		vec4 p = cpos;
		ps = pos.xyz;
		p.xy += (uv-vec2(.5))*.004;
		gl_Position = camToClip * p;
		EmitVertex();
	}
	EndPrimitive();
}
