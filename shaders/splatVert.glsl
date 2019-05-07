
#version 450

layout(std430) buffer points {vec2 coord[];};
layout(std430) buffer cols {vec4 col[];};

uniform vec2 screenCoord;
out vec3 res;
uniform float size;

void main() {
	const int quad = gl_VertexID/6;
	res = col[quad/2].xyz;
	if(dot(res, vec3(1.))==.0) res = vec3(1.);
	const vec2 uv = vec2(((gl_VertexID%2)==0)?1.:-1., ((((gl_VertexID+1)/3)%2)==0)?1.:-1.);
	gl_Position = vec4((coord[quad]*vec2(1., -1.)+uv*.5)*2./screenCoord-vec2(1.,-1.), .0, 1.);
}
