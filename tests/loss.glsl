
#version 450

layout(local_size_x = 16, local_size_y = 16) in;

layout(r16f) uniform image2DArray image;
layout(r16f) uniform image2DArray imageGradient;
layout(r16f) uniform image2DArray result;

void main() {
	ivec3 coord = ivec3(ivec2(i,j), gl_GlobalInvocationID.z);
	float val = imageLoad(result, coord).x-imageLoad(image, coord).x;
	imageStore(imageGradient, coord, vec4(val));
}
