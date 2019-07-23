
#version 450

uniform sampler2D result;
out vec3 color;

vec3 tonemap(vec3 color) {
	return color * ((1.+length(color)/(.1))/(1.+length(color)));
}

void main() {
	vec4 value = texelFetch(result, ivec2(gl_FragCoord.xy), 0);
	color = value.xyz/value.w;
}
