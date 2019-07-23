
#version 450

layout(local_size_x = 16, local_size_y = 16) in;

const int convSize = 7;
const int convCount = 1;

layout(r16f) uniform image2DArray image;
layout(r16f) uniform image2DArray imageGradient;
layout(r16f) uniform image2DArray result;
layout(r16f) uniform image2DArray resultGradient;

const float eps = .01;

void main() {
	

}
