
#version 450

in vec2 uv;
out vec3 color;

void main() {
	if(dot(uv,uv)>=1.) discard;
	color = vec3(1.);
}