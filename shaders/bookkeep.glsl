
#version 450
#extension GL_KHR_shader_subgroup_ballot: enable
#extension GL_KHR_shader_subgroup_arithmetic: enable

layout(local_size_x = 256) in;

uvec3 rndSeed;
int rndCounter = 0;
float rand() {
	if(rndCounter == 0) {
		rndSeed.x -= rndSeed.y; rndSeed.x -= rndSeed.z; rndSeed.x ^= rndSeed.z>>13;
		rndSeed.y -= rndSeed.z; rndSeed.y -= rndSeed.x; rndSeed.y ^= rndSeed.x<<8;
		rndSeed.z -= rndSeed.x; rndSeed.z -= rndSeed.y; rndSeed.z ^= rndSeed.y>>13;
		rndSeed.x -= rndSeed.y; rndSeed.x -= rndSeed.z; rndSeed.x ^= rndSeed.z>>12;
		rndSeed.y -= rndSeed.z; rndSeed.y -= rndSeed.x; rndSeed.y ^= rndSeed.x<<16;
		rndSeed.z -= rndSeed.x; rndSeed.z -= rndSeed.y; rndSeed.z ^= rndSeed.y>>5;
		rndSeed.x -= rndSeed.y; rndSeed.x -= rndSeed.z; rndSeed.x ^= rndSeed.z>>3;
		rndSeed.y -= rndSeed.z; rndSeed.y -= rndSeed.x; rndSeed.y ^= rndSeed.x<<10;
		rndSeed.z -= rndSeed.x; rndSeed.z -= rndSeed.y; rndSeed.z ^= rndSeed.y>>15;
	}
	return float(rndSeed[rndCounter = (rndCounter+1)%3])/pow(2.,32.);
}
void srand(uint A, uint B, uint C) {rndSeed = uvec3(A, B, C);  for(int j = 0; j<3; ++j) rand();}

const float pi = 3.14159265358979323;
bool expRandReady = false;
float expRandStore;
float exprand() {
	if(expRandReady) {
		expRandReady = false;
		return expRandStore;
	} else {
		expRandReady = true;
		float radius = sqrt(-2.*log(rand()));
		float angle = 2.*pi*rand();
		expRandStore = radius*cos(angle);
		return radius*sin(angle);
	}
}

layout(std430) buffer hashIndices{uint allocator, indices[];};
layout(std430) buffer hashCounts{uint counts[];};
layout(std430) buffer hashOffsets{uint offsets[];};

uniform int mode;

void main() {
	if(mode == 0) {
		for(uint i = gl_GlobalInvocationID.x; i<counts.length(); i += gl_NumWorkGroups.x * gl_WorkGroupSize.x)
			counts[i] = 0;
		if(gl_GlobalInvocationID.x == 0) allocator = 0;
	} else if(mode == 1) {
		for(uint i = gl_GlobalInvocationID.x; i<counts.length(); i += gl_NumWorkGroups.x * gl_WorkGroupSize.x) {
			uint local_offset = subgroupInclusiveAdd(counts[i]);
			uint global_offset;
			if(gl_SubgroupInvocationID == gl_SubgroupSize-1)
				global_offset = atomicAdd(allocator, local_offset);
			offsets[i] = local_offset + subgroupBroadcast(global_offset, gl_SubgroupSize-1);
		}
	}
}
