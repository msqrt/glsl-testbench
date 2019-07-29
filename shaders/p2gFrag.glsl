
#version 450

in vec2 uv;
in vec2 vel;
in mat2 aff;

in flat int write;
layout(std430) buffer posBuffer {vec2 particle_pos[];};
layout(std430) buffer velBuffer {vec2 particle_vel[];};

out vec4 density;
out vec2 velocity;

float bilin_kernel(vec2 p) {
	return max(1.-abs(p.x),.0)*max(1.-abs(p.y),.0);
}

void main() {
	vec3 w = vec3(bilin_kernel(uv+vec2(.5, .0)), bilin_kernel(uv+vec2(.0, .5)), bilin_kernel(uv));
	density = vec4(w,.0);
	velocity = w.xy*vel.xy;
	velocity.x -= w.x*dot(uv+vec2(.5,.0), aff[0]);
	velocity.y -= w.y*dot(uv+vec2(.0,.5), aff[1]);
}
