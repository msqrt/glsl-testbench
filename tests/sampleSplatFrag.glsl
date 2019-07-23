
#version 450

out float outFeature;
in float g_weight;
in vec3 g_uv;

uniform sampler2DArray kernel;

void main() {
	outFeature = g_weight*texture(kernel, g_uv).x;
}
