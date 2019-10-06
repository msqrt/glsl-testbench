
#version 450

layout (local_size_x = 16, local_size_y = 16) in;

layout(r32i)uniform iimage2D result;
layout(r32f)uniform image2D dist;

uniform int mode, frame;

uvec4 rndseed;
void jenkins_mix()
{
	rndseed.x -= rndseed.y; rndseed.x -= rndseed.z; rndseed.x ^= rndseed.z>>13;
	rndseed.y -= rndseed.z; rndseed.y -= rndseed.x; rndseed.y ^= rndseed.x<<8;
	rndseed.z -= rndseed.x; rndseed.z -= rndseed.y; rndseed.z ^= rndseed.y>>13;
	rndseed.x -= rndseed.y; rndseed.x -= rndseed.z; rndseed.x ^= rndseed.z>>12;
	rndseed.y -= rndseed.z; rndseed.y -= rndseed.x; rndseed.y ^= rndseed.x<<16;
	rndseed.z -= rndseed.x; rndseed.z -= rndseed.y; rndseed.z ^= rndseed.y>>5;
	rndseed.x -= rndseed.y; rndseed.x -= rndseed.z; rndseed.x ^= rndseed.z>>3;
	rndseed.y -= rndseed.z; rndseed.y -= rndseed.x; rndseed.y ^= rndseed.x<<10;
	rndseed.z -= rndseed.x; rndseed.z -= rndseed.y; rndseed.z ^= rndseed.y>>15;
}
void srand(uint A, uint B, uint C) {rndseed = uvec4(A, B, C, 0); jenkins_mix();}

float rand()
{
	if(0==rndseed.w++%3) jenkins_mix();
	rndseed.xyz = rndseed.yzx;
	return float(rndseed.x)/pow(2.,32.);
}

buffer furthestPoint { uint furthest; };

float norm(vec2 x) {
	//return abs(x.x)+abs(x.y);
	return length(vec2(x));
}

float toroid_length(const vec2 x, const vec2 y, const uvec2 size)
{
	float l = length(vec2(size));
	for(int i = -1; i<=1; ++i)
	for(int j = -1; j<=1; ++j)
	{
		const vec2 diff = x-y+vec2(float(i*size.x), float(j*size.y));
		l = min(norm(ivec2(diff)), l);
	}
	return l;
}

ivec2 getFurthest(uint f)
{
	return ivec2(f>>10, f) & 0x3FF;
}

uint constructFurthest(int x, int y, uint len)
{
	return (uint(len)<<20)|(uint(x)<<10)|uint(y);
}

uint constructCandidate(int x, int y, uint f)
{
	const ivec2 other_pt = getFurthest(f);
	const int len = int(norm(vec2(ivec2(x,y)-getFurthest(f))));
	return (uint(len)<<20)|(uint(x)<<10)|uint(y);
}

const uint bits = 0x1FF;

void main()
{
	const uvec2 size = imageSize(result).xy;
	if(gl_GlobalInvocationID.x>=size.x||gl_GlobalInvocationID.y>=size.y || frame>=size.x*size.y) return;

	srand(frame+1, gl_GlobalInvocationID.x+1, gl_GlobalInvocationID.y+1);
	
	switch(mode) {
		case 0:
		{
			ivec2 furthest_pt = ivec2(size.xy)/2;
			srand(frame,1,2);
			rand();
			if(gl_GlobalInvocationID.x+gl_GlobalInvocationID.y==0)
			{
				ivec2 scrambled = ivec2(uvec2(furthest_pt)^(rndseed.xy&bits));
				furthest = constructFurthest(scrambled.x, scrambled.y, 0);
			}
			imageStore(dist, ivec2(gl_GlobalInvocationID.xy), vec4(toroid_length(furthest_pt,vec2(gl_GlobalInvocationID.xy), size)));
			imageStore(result, ivec2(gl_GlobalInvocationID.xy), ivec4(-1));
		}
		break;
		case 1:
		{
			if(imageLoad(result, ivec2(gl_GlobalInvocationID.xy)).x<0)
			{
				uint len = uint(imageLoad(dist, ivec2(gl_GlobalInvocationID.xy)).x);
				srand(frame+1,1,2);
				rand();
				uint candidate = constructFurthest(int(gl_GlobalInvocationID.x^(rndseed.x&bits)), int(gl_GlobalInvocationID.y^(rndseed.y&bits)), len);
				if(candidate>furthest)
					atomicMax(furthest, candidate);
			}
		}
		break;
		case 2:
		{
			ivec2 furthest_pt = getFurthest(furthest);
			srand(frame,1,2);
			rand();
			furthest_pt = ivec2(uvec2(furthest_pt)^(rndseed.xy&bits));
			if(furthest_pt.x==gl_GlobalInvocationID.x && furthest_pt.y==gl_GlobalInvocationID.y)
			{
				furthest_pt = ivec2(uvec2(furthest_pt)^(rndseed.xy&bits));
				atomicMin(furthest, constructFurthest(furthest_pt.x, furthest_pt.y, 0));
				imageStore(result, ivec2(gl_GlobalInvocationID.xy), ivec4(frame));
			}
			else if(imageLoad(result, ivec2(gl_GlobalInvocationID.xy)).x<0)
			{
				float l = imageLoad(dist, ivec2(gl_GlobalInvocationID.xy)).x;
				l = min(l, toroid_length(vec2(furthest_pt.x,furthest_pt.y),vec2(gl_GlobalInvocationID.xy), size)+rand()*.1);
				imageStore(dist, ivec2(gl_GlobalInvocationID.xy), vec4(l));
			}
		}
		break;
	}
}
