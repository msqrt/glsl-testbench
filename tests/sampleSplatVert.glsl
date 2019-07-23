
#version 450

layout(r16f) uniform image2D matrix;
layout(r16f) uniform image2DArray inputImage;
layout(r16f) uniform image2DArray splat;

out float weight;
out vec3 uv;
out int feature;

uniform int features;
uniform int featureOffset;
uniform int res;

void main() {
	const int quad = gl_VertexID/6;
	feature = quad % features;
	const int coordInd = quad / features;
	ivec2 coord = ivec2(coordInd%res, coordInd/res);
	weight = .0;
	for(int i = 0; i<imageSize(inputImage).z; ++i)
		weight += imageLoad(inputImage, ivec3(coord.xy, i)).x*imageLoad(matrix, ivec2(i, featureOffset + feature)).x;
	weight = max(weight, .0);
	ivec2 splatSize = imageSize(splat).xy;
	uv = vec3(((gl_VertexID%2)==0)?1.:.0, ((((gl_VertexID+1)/3)%2)==0)?1.:.0, float(feature)+1.);
	gl_Position = vec4((vec2(coord)+(uv.xy-vec2(.5))*vec2(splatSize.xy))/res*2.-vec2(1.), .0, 1.);
}
