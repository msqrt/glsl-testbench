
#version 450

layout(std430) buffer bounds {vec4 box[];};
layout(std430) buffer ranges {ivec2 interval[];};

out vec4 boundingBox;
out flat ivec2 range;

uniform vec2 screenSize;

void main() {
	const int quad = gl_VertexID/6;
	range = interval[quad];
	boundingBox = box[quad];
	vec4 bbox = boundingBox;
	bbox.xy = floor(bbox.xy);
	bbox.zw = ceil(bbox.zw);
	const vec2 uv = vec2(((gl_VertexID%2)==0)?bbox.z:bbox.x, screenSize.y-(((((gl_VertexID+1)/3)%2)==0)?bbox.w:bbox.y));
	gl_Position = vec4(uv/screenSize*2.-vec2(1.), .0, 1.);
}
