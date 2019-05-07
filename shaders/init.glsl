
#version 450

layout(local_size_x = 256) in;
const int lockstep = 32;

layout(std430) buffer points{float pos[];};
layout(std430) buffer extents{float ext[];};

uvec3 mix(uvec3 seed) {
  seed.x -= seed.y; seed.x -= seed.z; seed.x ^= (seed.z>>13);
  seed.y -= seed.z; seed.y -= seed.x; seed.y ^= (seed.x<<8); 
  seed.z -= seed.x; seed.z -= seed.y; seed.z ^= (seed.y>>13);
  seed.x -= seed.y; seed.x -= seed.z; seed.x ^= (seed.z>>12);
  seed.y -= seed.z; seed.y -= seed.x; seed.y ^= (seed.x<<16);
  seed.z -= seed.x; seed.z -= seed.y; seed.z ^= (seed.y>>5); 
  seed.x -= seed.y; seed.x -= seed.z; seed.x ^= (seed.z>>3); 
  seed.y -= seed.z; seed.y -= seed.x; seed.y ^= (seed.x<<10);
  seed.z -= seed.x; seed.z -= seed.y; seed.z ^= (seed.y>>15);
  return seed;
}

uvec3 rndSeed;
uint seedCounter;
void srnd(uvec3 seed) { rndSeed = mix(seed); seedCounter = 0; }
float rnd() {
	if(((seedCounter++) % 3) == 0) rndSeed = mix(rndSeed);
	return float(double((rndSeed = rndSeed.yzx).x)*double(pow(.5, 32.)));
}

uniform float t;

const int maxdim = 3;
uniform int N, dim;

void main() {

	for(int thread = int(gl_GlobalInvocationID.x); thread<N; thread+=int(gl_WorkGroupSize.x*gl_NumWorkGroups.x)) {
		srnd(uvec3(1u, int(t*.0+2.), uint(thread)));
		vec3 p = vec3(rnd(), rnd(), rnd())*2.-vec3(1.);
		p.yz *= .2;
		float c = cos(t*.2), s = sin(t*.2);
		p.xz = mat2(c, s, -s, c) * p.xz;
		for(int i = 0; i<dim; ++i) {
			pos[thread+i*N] = p[i];
			ext[thread+i*N] = .002;
		}
	}
}
