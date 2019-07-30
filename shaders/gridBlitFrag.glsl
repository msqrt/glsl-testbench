
#version 450

uniform sampler3D shade;
in vec2 uv;
out vec3 color;

uniform float t;

void main() {
	color = vec3(.0);//texture(shade, vec3(uv*.5+vec2(.5), .15).xzy).xyz;
}
