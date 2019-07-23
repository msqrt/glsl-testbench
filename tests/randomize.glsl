
#version 450

layout(local_size_x = 8, local_size_y = 8) in;

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
uint boxMullerCounter = 0;
vec2 boxMullerVal;
float normrnd() {
	if(((boxMullerCounter++)%2) == 0) {
		float ang = 2.*3.14159265358979*rnd();
		boxMullerVal = sqrt(-2.*log(1.-rnd()))*vec2(cos(ang), sin(ang));
	}
	return (boxMullerVal = boxMullerVal.yx).x;
}

layout(r16f) uniform image2D matrix;
layout(r16f) uniform image2DArray upscale;
layout(r16f) uniform image2DArray splat;

layout(r16f) uniform image2D baseMatrix;
layout(r16f) uniform image2DArray baseUpscale;
layout(r16f) uniform image2DArray baseSplat;

uniform int frame;
uniform float rate;

void main() {
	srnd(uvec3(gl_GlobalInvocationID.xyz)+uvec3(frame));
	ivec2 matrixSize = imageSize(matrix);
	ivec3 thread = ivec3(gl_GlobalInvocationID);
	if(thread.y==0 && thread.x<matrixSize.x) {
		if(frame == 0) {
			imageStore(baseMatrix, thread.xz, .001*vec4(normrnd()));
			imageStore(matrix, thread.xz, .001*vec4(normrnd()));
		}
		else// if(rnd()<rate)
			imageStore(matrix, thread.xz, vec4(.001*rate*normrnd()+imageLoad(baseMatrix, thread.xz).x));
	}
	
	ivec3 upscaleSize = imageSize(upscale);
	if(thread.x<upscaleSize.x && thread.y<upscaleSize.y) {
		if(frame == 0) {
			imageStore(baseUpscale, thread, vec4(normrnd()));
			imageStore(upscale, thread, vec4(normrnd()));
		}
		else// if(rnd()<rate)
			imageStore(upscale, thread, vec4(rate*normrnd()+imageLoad(baseUpscale, thread).x));
	}
	
	ivec3 splatSize = imageSize(splat);
	if(thread.x<splatSize.x && thread.y<splatSize.y) {
		if(frame == 0) {
			imageStore(baseSplat, thread, .1*vec4(normrnd()));
			imageStore(splat, thread, .1*vec4(normrnd()));
		}
		else// if(rnd()<rate)
			imageStore(splat, thread, vec4(.1*rate*normrnd()+imageLoad(baseSplat, thread).x));
	}
}
