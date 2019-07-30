
#version 450

layout(points) in;
layout(triangle_strip, max_vertices=12) out;

layout(std430) buffer posBuffer {vec4 particle_pos[];};
layout(std430) buffer velBuffer {vec4 particle_vel[];};
layout(std430) buffer affBuffer {vec4 affine[];};

out vec3 pos;
out vec3 vel;
out mat3 aff;

uniform int res;

void main() {
	
	vec3 p = particle_pos[gl_PrimitiveIDIn].xyz;
	vec3 velocity = particle_vel[gl_PrimitiveIDIn].xyz;
	mat3 aff_map;
	for(int i = 0; i<3; ++i)
		aff_map[i] = affine[gl_PrimitiveIDIn*3+i].xyz;

	ivec3 cell = ivec3(p*vec3(res))-ivec3(1);

	for(int i = 0; i<3*4; ++i) {
		pos = p;
		vel = velocity;
		aff = aff_map;
		vec2 uv = vec2(float(i%2), float((i%4)/2));
		vec2 splat_pos = vec2(cell.xy) + uv*3;
		gl_Position = vec4(splat_pos/vec2(res)*2.-vec2(1.), .0, 1.0);
		gl_Layer = cell.z+i/4;
		EmitVertex();
		if((i&3)==3)
			EndPrimitive();
	}
}
