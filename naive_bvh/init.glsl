
#version 450

layout(local_size_x = 256) in;
const int lockstep = 32;

layout(std430) buffer points{float pos[];};
layout(std430) buffer extents{float ext[];};
layout(std430) buffer indices{int index[];};
layout(std430) buffer nodes{int node[];};

layout(std430) buffer buildExtents{uvec2 nodeExt[];};
layout(std430) buffer buildIndices{int alloc, children[];};
layout(std430) buffer buildSizes{int size[];};
layout(std430) buffer buildParents{int parent[];};

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
void srnd(uvec3 seed) {
	rndSeed = mix(mix(seed));
	seedCounter = 0;
}
float rnd() {
	if(((seedCounter++) % 3) == 0) rndSeed = mix(rndSeed);
	return float(double((rndSeed = rndSeed.yzx).x)*double(pow(.5, 32.)));
}

uniform float t;

uint sortable(float x) {
	return floatBitsToUint(x) ^ uint((-int(floatBitsToUint(x)>>31))|0x80000000);
}
float unsortable(uint x) {
	return uintBitsToFloat(x ^ (((x>>31)-1)|0x80000000));
}

void main() {
	const int N = node.length();
	const int dim = pos.length()/node.length();

	const int thread = int(gl_GlobalInvocationID.x);
	srnd(uvec3(1u, int(t*.0+2.), gl_GlobalInvocationID.x));
	if(thread<N) {
		vec3 p = vec3(rnd(), rnd(), rnd())*2.-vec3(1.);
		p.yz *= .2;
		float c = cos(t*.2), s = sin(t*.2);
		p.xz = mat2(c, s, -s, c) * p.xz;
		for(int i = 0; i<dim; ++i) {
			pos[thread+i*N] = p[i];
			atomicMin(nodeExt[i].x, sortable(p[i]));
			atomicMax(nodeExt[i].y, sortable(p[i]));
			ext[thread+i*N] = .001;
		}
		//index[thread] = thread;
		node[thread] = 0;
	}
	if(thread<N-1) {
		size[thread] = (thread == 0)?N:0;
		if(thread>0 && thread<3)
			parent[thread] = 0;
		else
			parent[thread] = -1;
		if(thread>0)
			for(int i = 0; i<dim; ++i)
				nodeExt[thread*dim+i] = uvec2(sortable(3.4e38), sortable(-3.4e38));
	}
	if(thread==0) {
		children[0] = 1;
		alloc = 3;
	}
}
