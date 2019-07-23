
#version 450
#extension GL_KHR_shader_subgroup_ballot: enable
#extension GL_KHR_shader_subgroup_shuffle: enable
#extension GL_KHR_shader_subgroup_arithmetic: enable

layout(local_size_x = 256) in;

uvec3 mix(uvec3 seed) {
	seed.x -= seed.y; seed.x -= seed.z; seed.x ^= seed.z>>13;
	seed.y -= seed.z; seed.y -= seed.x; seed.y ^= seed.x<<8;
	seed.z -= seed.x; seed.z -= seed.y; seed.z ^= seed.y>>13;
	seed.x -= seed.y; seed.x -= seed.z; seed.x ^= seed.z>>12;
	seed.y -= seed.z; seed.y -= seed.x; seed.y ^= seed.x<<16;
	seed.z -= seed.x; seed.z -= seed.y; seed.z ^= seed.y>>5;
	seed.x -= seed.y; seed.x -= seed.z; seed.x ^= seed.z>>3;
	seed.y -= seed.z; seed.y -= seed.x; seed.y ^= seed.x<<10;
	seed.z -= seed.x; seed.z -= seed.y; seed.z ^= seed.y>>15;
	return seed;
}

layout(std430) buffer inputBuffer{vec2 data[];};
layout(std430) buffer outputBuffer{vec2 result[];};
uniform int mode;

void main() {
	switch(mode) {
		case -1: // init randoms
		{
			for(uint i = gl_GlobalInvocationID.x; i<data.length(); i+=gl_WorkGroupSize.x*gl_NumWorkGroups.x)
				data[i] = mix(uvec3(1,2,i)).xy;
		}break;
		case 0: // memcpy
		{
			for(uint i = gl_GlobalInvocationID.x; i<data.length(); i+=gl_WorkGroupSize.x*gl_NumWorkGroups.x)
				result[i] = data[i];
		}break;
		case 1: // 
		{
			for(uint i = gl_GlobalInvocationID.x; i<data.length(); i+=gl_WorkGroupSize.x*gl_NumWorkGroups.x)
				result[i] = data[i];
		}break;
	}
}
