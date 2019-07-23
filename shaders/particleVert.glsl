
#version 450

layout(std430) buffer posBuffer{vec4 position[];};
layout(std430) buffer velBuffer{vec4 velocity[];};

uniform mat4 worldToCamera, cameraToClip;
const float pi = 3.14159265358979323;

out vec2 uv;

void main() {
	const int particleID = gl_VertexID/3;

	vec3 p = position[particleID].xyz;

	vec3 camCoord = (worldToCamera * vec4(p, 1.)).xyz;
	float r = .001;

	float angle = float(gl_VertexID%3)*2.*pi/3.;
	uv = 2.*vec2(cos(angle), sin(angle));

	camCoord.xy += r*uv;
	gl_Position = cameraToClip * vec4(camCoord, 1.);
}
