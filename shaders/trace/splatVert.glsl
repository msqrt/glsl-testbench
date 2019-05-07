
#version 450

uniform ivec2 screenSize;
uniform int frame;

buffer values{vec4 color[];};
layout(std430)buffer coordinates{vec2 coord[];};

out vec4 value;
out vec2 uv;

void main() {
	const uint quad = gl_VertexID/6;
	
	uv = vec2(((gl_VertexID%2)==0)?-2.:2., ((((gl_VertexID+2)/3)%2)==0)?-2.:2.);

	value = color[quad];
	if(isnan(dot(value, vec4(1.)))) value = vec4(.0);

	vec2 coord = coord[quad];
	gl_Position = vec4(coord*2.-vec2(1.)+uv*2./vec2(screenSize), .0, 1.);
}
