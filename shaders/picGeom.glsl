
#version 450

layout(points) in;
layout(triangle_strip, max_vertices=4) out;

layout(std430) buffer posBuffer {vec4 particle_pos[];};
layout(std430) buffer velBuffer {vec4 particle_vel[];};

uniform mat4 worldToCam;
uniform mat4 camToClip;
out vec2 uv;
out vec3 ps;
out vec3 vel;

uniform int scene;
uniform float t;


void main() {
	
	vec4 pos = vec4(particle_pos[gl_PrimitiveIDIn].xyz, 1.);
	vec4 velocity = particle_vel[gl_PrimitiveIDIn];

	vec3 orig, target, up = vec3(.0,1.,.0);
	float fov = 1.1;

	vec4 cpos = worldToCam * pos;
	vec4 cvpos = worldToCam * (pos+vec4(velocity.xyz,.0)*.1);

	for(int i = 0; i<4; ++i) {
		vel = velocity.xyz;
		vec2 diff = (cvpos-cpos).xy;
		vec2 adiff = vec2(diff.y,-diff.x);
		uv = vec2(float(i%2), float(i/2));
		vec4 p = cpos;
		ps = pos.xyz;
		float l = min(3.,max(1.,200.*length(diff)));
		p.xy += (mat2(normalize(diff)*l,normalize(adiff)/l))*(uv-vec2(.5))*.005;
		gl_Position = camToClip * p;
		EmitVertex();
	}
	EndPrimitive();
}
