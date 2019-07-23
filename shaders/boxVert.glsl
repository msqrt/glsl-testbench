
#version 450

layout(std430) buffer extents{vec2 minMax[];};
layout(std430) buffer bones{mat4 boneToWorld[];}; // objToBone as well

uniform mat4 worldToCamera, camToClip;

out vec2 uv;

void main() {
	const int cube = gl_VertexID/36;

	const int face = gl_VertexID/6;
	const int uvind = gl_VertexID%6;
	vec3 mins, maxs;
	for(int i = 0; i<3; ++i) {
		vec2 mm = minMax[cube*3+i];
		mins[i] = mm[0];
		maxs[i] = mm[1];
	}
	vec3 local = vec3(float(uvind%2), float(((uvind+1)/3)%2), 1.);
	uv = local.xy;
	local -= vec3(.5);
	if(face>2) local.yz = -local.yz;
	switch(face%3) {
		case 0: local.xyz = local.yzx; break;
		case 1: local.xyz = local.zyx; break;
		default: break;
	}
	vec3 pos = mins + (maxs-mins)*local;
	gl_Position = camToClip * worldToCamera * boneToWorld[cube] * vec4(pos, 1.);
}
