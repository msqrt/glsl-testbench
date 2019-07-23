
#version 450

layout(local_size_x = 256) in;

uvec3 rndSeed;
int rndCounter = 0;
void mix() {
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

uint hash(uvec3 cell) {
	return (73856093 * cell.x) ^ (19349663*cell.y) ^ (83492791*cell.z);
}

uint hash_2(uvec3 cell) {
	cell.x -= cell.y; cell.x -= cell.z; cell.x ^= cell.z>>13;
	cell.y -= cell.z; cell.y -= cell.x; cell.y ^= cell.x<<8;
	cell.z -= cell.x; cell.z -= cell.y; cell.z ^= cell.y>>13;
	cell.x -= cell.y; cell.x -= cell.z; cell.x ^= cell.z>>12;
	cell.y -= cell.z; cell.y -= cell.x; cell.y ^= cell.x<<16;
	cell.z -= cell.x; cell.z -= cell.y; cell.z ^= cell.y>>5;
	cell.x -= cell.y; cell.x -= cell.z; cell.x ^= cell.z>>3;
	return cell.x;
}

float rand() {
	if(rndCounter == 0) mix();
	return float(rndSeed[rndCounter = (rndCounter+1)%3])/pow(2.,32.);
}
void srand(uint A, uint B, uint C) {rndSeed = uvec3(A, B, C); rndCounter = 0; for(int j = 0; j<1; ++j) mix();}

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

layout(std430) buffer posBuffer{vec4 position[];};
layout(std430) buffer velBuffer{vec4 velocity[];};
layout(std430) buffer posBufferOut{vec4 positionOut[];};
layout(std430) buffer velBufferOut{vec4 velocityOut[];};
layout(std430) buffer hashIndices{uint allocator, indices[];};
layout(std430) buffer hashCounts{uint counts[];};
layout(std430) buffer hashOffsets{int offsets[];};

uniform int mode;

uniform float dt;

void main() {
	int hash_size = counts.length();
	const float r = .01;
	if(mode == 0) {
		for(uint i = gl_GlobalInvocationID.x; i<position.length(); i += gl_NumWorkGroups.x * gl_WorkGroupSize.x) {
			srand(i, 1, 2);
			position[i] = vec4(rand(), rand(), rand(), 1.)*2.-vec4(1.);
			velocity[i] = .0*vec4(normalize(vec3(exprand(), exprand(), exprand()))*.1, 1.);
		}
	} else if(mode==1) {
		for(uint i = gl_GlobalInvocationID.x; i<position.length(); i += gl_NumWorkGroups.x * gl_WorkGroupSize.x) {
			vec4 pos = position[i];
			vec4 vel = velocity[i];
			
			ivec3 ipos = ivec3((pos.xyz/r));
			for(int j = 0; j<27; ++j) {
				uint h = hash(uvec3(ipos)+ivec3(j%3, (j/3)%3, (j/9)%3))%hash_size;
				uint first = offsets[h], last = first + counts[h];
				for(uint k = first; k<last; ++k) {
					uint other = indices[k];
					if(other == i) continue;
					vec4 otherpos = position[other];
					vec4 othervel = velocity[other];
					vec3 diff = (otherpos-pos).xyz;
					if(length(diff)<r) {
						vel.xyz -= (diff)*.1;
					}
				}
			}

			pos.xyz += dt*vel.xyz;

			for(int j = 0; j<3; ++j)
				if(abs(pos[j])>=1.) {
					pos[j] -= dt*vel[j];
					vel[j] *= -1.;
				}
				
			positionOut[i] = pos;
			velocityOut[i] = vel;
		}
	} else if(mode == 2) {
		for(uint i = gl_GlobalInvocationID.x; i<position.length(); i += gl_NumWorkGroups.x * gl_WorkGroupSize.x) {
			vec4 pos = position[i];
			ivec3 ipos = ivec3((pos.xyz/r));
			uint h = hash(uvec3(ipos));
			atomicAdd(counts[h%hash_size], 1);
		}
	} else if(mode == 3) {
		for(uint i = gl_GlobalInvocationID.x; i<position.length(); i += gl_NumWorkGroups.x * gl_WorkGroupSize.x) {
			vec4 pos = position[i];
			ivec3 ipos = ivec3((pos.xyz/r));
			uint h = hash(uvec3(ipos));
			indices[atomicAdd(offsets[h%hash_size], -1)-1] = i;
		}
	}
}
