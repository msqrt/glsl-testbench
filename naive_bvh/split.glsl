
#version 450

layout(local_size_x = 256) in;

layout(std430) restrict buffer points{float pos[];};
layout(std430) buffer indices{int index[];};
layout(std430) buffer nodes{int treeNode[];};

layout(std430) buffer buildExtents{uvec2 ext[];};
layout(std430) buffer buildIndices{int alloc, children[];};
layout(std430) buffer buildSizes{int size[];};
layout(std430) buffer buildParents{int parent[];};

uint sortable(float x) {
	return floatBitsToUint(x) ^ uint((-int(floatBitsToUint(x)>>31))|0x80000000);
}
float unsortable(uint x) {
	return uintBitsToFloat(x ^ (((x>>31)-1)|0x80000000));
}
vec2 unsortable(uvec2 x) {
	return vec2(unsortable(x.x), unsortable(x.y));
}

const int N = treeNode.length();
const int dim = pos.length()/treeNode.length();

void main() {
	int thread = int(gl_GlobalInvocationID.x);
	if(thread<N) {
		int node = treeNode[thread];
		if(size[node]>16) {
			vec3 p;
			for(int i = 0; i<dim; ++i)
				p[i] = pos[thread+i*N];
			
			vec3 n = vec3(.0); float o, maxExt = .0; vec3 id = vec3(1., .0, .0);
			for(int i = 0; i<dim; ++i) {
				vec2 interval = unsortable(ext[node*dim+i]);
				if(interval.y-interval.x>maxExt) {
					maxExt = interval.y-interval.x;
					n = id;
					o = .5*(interval.x+interval.y);
				}
				id = id.zxy;
			}
			int offset = int(floatBitsToUint(dot(n, p)-o)>>31);
			int child = children[node]+offset;
			for(int i = 0; i<dim; ++i) {
				atomicMin(ext[child*dim+i].x, sortable(p[i]));
				atomicMax(ext[child*dim+i].y, sortable(p[i]));
			}
			atomicAdd(size[child], 1);
			if(1==atomicAdd(size[node], -1))
				for(int i = child-offset; i<child-offset+2; ++i)
					if(size[i]>16) {
						int kid = children[i] = atomicAdd(alloc, 2);
						parent[kid] = parent[kid+1] = i;
					}
			treeNode[thread] = child;
		}
	}
}
