
#version 450

layout(local_size_x = 16, local_size_y = 16) in;

layout(r32ui) uniform uimage2D outBuff;

void main() {
	ivec2 size = imageSize(outBuff).xy;
	if(gl_GlobalInvocationID.x<size.x && gl_GlobalInvocationID.y<size.y)
		imageStore(outBuff, ivec2(gl_GlobalInvocationID.xy), uvec4(0u));
}
