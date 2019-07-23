
#version 450

layout(local_size_x = 256) in;

layout(r16f) uniform image2DArray image;
layout(r16f) uniform image2DArray target;

layout(std430) buffer difference{uint diff[];};

const mat3 opponent = mat3(1./3., 1./sqrt(6.), 1./(3.*sqrt(2.)),1./3., .0, -sqrt(2.)/3., 1./3., -1./sqrt(6.), 1./(3.*sqrt(2.)));

shared float adder[gl_WorkGroupSize.x];

void main() {
	
	ivec3 size = imageSize(image);
	adder[gl_LocalInvocationID.x] = .0;
	for(int i = int(gl_GlobalInvocationID.x); i<size.x*size.y; i+=int(gl_WorkGroupSize.x*gl_NumWorkGroups.x)) {
		ivec2 coord = ivec2(i/size.x, i%size.x);
		vec3 res, truth;
		for(int j = 0; j<3; ++j) {
			res[j] = clamp(imageLoad(image, ivec3(coord, j)).x, .0, 1.);
			truth[j] = clamp(imageLoad(target, ivec3(coord, j)).x, .0, 1.);
		}
		res = (opponent * res - truth);
		float cur = dot(vec3(1.), abs(res));
		//if(!isnan(cur) && !isinf(cur))
			adder[gl_LocalInvocationID.x] += cur;
	}
	for(int i = int(gl_WorkGroupSize.x)/2; i>0; i /= 2) {
		barrier();
		if(gl_LocalInvocationID.x<i)
			adder[gl_LocalInvocationID.x] += adder[gl_LocalInvocationID.x+i];
	}
	barrier();
	if(gl_LocalInvocationID.x==0) {
		uint assumed = 0, old = diff[0];
		do {
			assumed = old;
			old = atomicCompSwap(diff[0], assumed, floatBitsToUint(adder[0]+uintBitsToFloat(assumed)));
		} while(old != assumed);
	}
}
