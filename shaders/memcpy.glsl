
#version 460
layout(local_size_x = 256) in;

layout(std430) buffer inputBuffer{uvec4 data[];};
layout(std430) buffer outputBuffer{uvec4 result[];};
layout(std430) buffer inputScalarBuffer{uint scalar_data[];};
layout(std430) buffer outputScalarBuffer{uint scalar_result[];};

void main() {
	for(uint i = gl_GlobalInvocationID.x; i<data.length(); i+=gl_WorkGroupSize.x*gl_NumWorkGroups.x)
		result[i] = data[i];
	uint i = data.length()*4+gl_GlobalInvocationID.x;
	if(i<scalar_data.length())
		scalar_result[i] = scalar_data[i];
}
