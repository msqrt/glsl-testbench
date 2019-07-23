
#version 450

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba16f) uniform writeonly image2D result;

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

const float pi = 3.14159265358979;
mat3 basis(vec3 n) {
    float sg = uintBitsToFloat(floatBitsToUint(1.)|((1<<31)&floatBitsToUint(n.z)));
    float a = -1.0 / (sg + n.z);
    float b = n.x * n.y * a;
    return mat3(vec3(1.0 + sg * n.x * n.x * a, sg * b, -sg * n.x), vec3(b, sg + n.y * n.y * a, -n.y), n);
}

uniform mat4 cameraToWorld, clipToCamera;
uniform uint tick;

float dist(vec3 p) {
	return min(length(p)-2., p.y-.1);
}

vec3 gradient(vec3 p) {
	vec2 eps = vec2(.001, .0);
	return vec3(dist(p+eps.xyy)-dist(p-eps.xyy), dist(p+eps.yxy)-dist(p-eps.yxy), dist(p+eps.yyx)-dist(p-eps.yyx))/(2.*eps.x);
}

float trace(vec3 o, vec3 d) {
	float t = .0, g; int k = 0;
	do {
		g = dist(o);
		t += g;
		o += g * d;
	} while( g>.001*t && k++<80);
	return t;
}

void main() {
	ivec2 screenSize = imageSize(result);
	if(gl_GlobalInvocationID.x >= screenSize.x || gl_GlobalInvocationID.y >= screenSize.y)
		return;
	srand(tick, gl_GlobalInvocationID.x+1, gl_GlobalInvocationID.y+1);

	vec2 uv = vec2(gl_GlobalInvocationID.xy)/vec2(screenSize)*2.-vec2(1.);
	vec3 origin = (cameraToWorld * vec4(vec3(.0), 1.)).xyz;
	vec4 clip_start = clipToCamera * vec4(uv, .0, 1.), clip_end = clipToCamera * vec4(uv, 1., 1.);
	vec3 direction = mat3(cameraToWorld) * normalize(clip_end.xyz/clip_end.w-clip_start.xyz/clip_start.w);

	float t = trace(origin, direction);
	vec3 p = origin + t *.99* direction;
	float t2 = trace(p, normalize(vec3(.2, 1., .5)));
	bool shadow = false;
	if(t2<20.) shadow = true;
	vec3 col = vec3(.0);
	if(!shadow)
		col = vec3(.8, .2, .1)*max(.0, dot(normalize(gradient(p)), normalize(vec3(.2, 1., .5))));

	imageStore(result, ivec2(gl_GlobalInvocationID.xy), vec4(col+vec3(rand())*.001, 1.));
}
