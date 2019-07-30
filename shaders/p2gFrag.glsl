
#version 450

in vec3 pos;
in vec3 vel;
in mat3 aff;

out vec4 density;
out vec3 velocity;

uniform int res;

float bilin_kernel(vec3 p) {
	return max(1.-abs(p.x),.0)*max(1.-abs(p.y),.0)*max(1.-abs(p.z),.0);
}

void main() {
	vec3 uv = (pos*float(res)-vec3(gl_FragCoord.xy, float(gl_Layer)+.5));
	density = vec4(bilin_kernel(uv+vec3(.5, .0, .0)), bilin_kernel(uv+vec3(.0, .5, .0)), bilin_kernel(uv+vec3(.0, .0, .5)), bilin_kernel(uv));
	velocity = density.xyz*vel.xyz;
	velocity.x -= density.x*dot(uv+vec3(.5,.0,.0), aff[0]);
	velocity.y -= density.y*dot(uv+vec3(.0,.5,.0), aff[1]);
	velocity.z -= density.z*dot(uv+vec3(.0,.0,.5), aff[2]);
}
