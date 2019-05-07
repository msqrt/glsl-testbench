
#version 450

layout(local_size_x = 256) in;
const int lockstep = 32;

layout(std430) buffer points{float pos[];};
layout(std430) buffer extents{float ext[];};
layout(std430) buffer indices{int ind[];};

layout(std430) buffer buildExtents{uvec2 aabb[];};
layout(std430) buffer buildIndices{int childAlloc, children[];};
layout(std430) buffer buildSizes{int size[];};
layout(std430) buffer buildParents{int parent[];};

layout(std430) buffer threadCounter{ int threads, current; };

uint sortable(float x) { return floatBitsToUint(x) ^ uint((-int(floatBitsToUint(x)>>31))|0x80000000); }
uvec2 sortable(vec2 x) { return uvec2(sortable(x.x), sortable(x.y)); }
float unsortable(uint x) { return uintBitsToFloat(x ^ (((x>>31)-1)|0x80000000)); }
vec2 unsortable(uvec2 x) { return vec2(unsortable(x.x), unsortable(x.y)); }

uniform int N;
const int dim = 3;
const int local_thread = int(gl_LocalInvocationIndex), thread = int(gl_GlobalInvocationID.x);

shared int workAlloc, offset;

void main() {
	if(local_thread==0) workAlloc = current;
	barrier();
	if(workAlloc>=N) return;
	if(local_thread==0)
		offset = atomicAdd(threads, int(gl_WorkGroupSize.x));

	barrier();
	{
		float mins[dim], maxs[dim];
		for(int i = 0; i<dim; ++i) {
			mins[i] = 3.4e38;
			maxs[i] = -3.4e38;
		}
		int count = 0;
		while(true) {
			if(local_thread==0u)
				workAlloc = atomicAdd(current, int(gl_WorkGroupSize.x));
			barrier();
			if(workAlloc>=N)
				break;
			const int index = workAlloc + local_thread;
			barrier();
			
			if(index<N) {
				count++;
				ind[index] = index;
				for(int i = 0; i<dim; ++i) {
					float s = pos[index+i*N];
					mins[i] = min(mins[i], s);
					maxs[i] = max(maxs[i], s);
				}
				if(index<N-1) {
					parent[index] = (index==1||index==2)?0:-1;
					children[index] = 0;
					if(index>0)
						size[index] = 0;
					for(int i = 0; i<dim; ++i)
						aabb[index+i*N] = sortable(vec2(3.4e38, -3.4e38));
				}
				if(index==0)
					childAlloc = 1;
			}
		}
		atomicAdd(size[0], count);
		while(atomicAdd(size[0], 0)<N); // global barrier
		for(int i = 0; i<dim; ++i) {
			atomicMin(aabb[i*N].x, sortable(mins[i]));
			atomicMax(aabb[i*N].y, sortable(maxs[i]));
		}
	}
		
	int node = 0;
	int start = 0, end = N;

	int activeThreads = threads, localOffset = offset + local_thread;
	int ping = 0, pong = N*dim; // partition ping into pong
	while(activeThreads>int(gl_WorkGroupSize.x)) {
			
		if(localOffset==0)
			children[node] = atomicAdd(childAlloc, 2);
		int left;
		while((left = atomicAdd(children[node], 0))==0); // global barrier
		const int right = left + 1;

		int splitDimension = 0; float splitPlane, splitExt = -1.;
		float leftMins[dim], leftMaxs[dim], rightMins[dim], rightMaxs[dim];
		for(int i = 0; i<dim; ++i) {
			leftMins[i] = rightMins[i] = 3.4e38;
			leftMaxs[i] = rightMaxs[i] = -3.4e38;
		}
		for(int j = 0; j<dim; ++j) {
			vec2 interval = unsortable(aabb[node+j*N]);
			float curExt = interval.y-interval.x;
			if(curExt>splitExt) {
				splitExt = curExt;
				splitPlane = (interval.x+interval.y)*.5;
				splitDimension = j;
			}
		}
		int leftSize, rightSize;
		for(int i = start+localOffset; i<end; i+=activeThreads) {
			bool isLeft; float splitS;
			int addr;
			if((splitS = pos[ping+i+splitDimension*N])<splitPlane) {
				isLeft = true;
				leftSize = atomicAdd(size[left], 1);
				addr = start + leftSize;
			} else {
				isLeft = false;
				rightSize = atomicAdd(size[right], 1);
				addr = end - 1 - rightSize;
			}
			for(int j = 0; j<dim; ++j) {
				float s;
				if(j == splitDimension)
					s = splitS;
				else
					s = pos[ping+i+j*N];
				if(isLeft) {
					leftMins[j] = min(leftMins[j], s);
					leftMaxs[j] = max(leftMaxs[j], s);
				} else {
					rightMins[j] = min(rightMins[j], s);
					rightMaxs[j] = max(rightMaxs[j], s);
				}
				pos[pong+addr+j*N] = s;
			}
		}
		for(int i = 0; i<dim; ++i) {
			atomicMin(aabb[left+i*N].x, sortable(leftMins[i]));
			atomicMax(aabb[left+i*N].y, sortable(leftMaxs[i]));
			atomicMin(aabb[right+i*N].x, sortable(rightMins[i]));
			atomicMax(aabb[right+i*N].y, sortable(rightMaxs[i]));
		}

		while(leftSize+rightSize<end-start) {
			leftSize = atomicAdd(size[left], 0);
			rightSize = atomicAdd(size[right], 0);
		}

		int split = int(float(leftSize)/float(leftSize+rightSize)*activeThreads);
		split = (split/int(gl_WorkGroupSize.x))*int(gl_WorkGroupSize.x);

		if(localOffset<split) {
			node = left;
			end = start+leftSize;
			activeThreads = split;
		} else {
			node = right;
			start += leftSize;
			activeThreads = activeThreads - split;
			localOffset -= split;
		}
		int tmp = pong;
		pong = ping;
		ping = tmp;
		break;
	}
}
