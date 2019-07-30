
#version 450

in vec2 uv;
in vec3 ps;
out vec3 col;
uniform sampler3D shade;

void main() {
	if(length(uv-vec2(.5))>.5) discard;
	col = texture(shade,ps+vec3(.0,.01,.0)).xyz;
}
