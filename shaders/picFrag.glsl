
#version 450

in vec2 uv;
in vec3 ps;
out vec3 col;
uniform sampler3D shade;

void main() {
	if(length(uv-vec2(.5))>.5) discard;
	col = vec3(.0);
	for(int i = 0; i<4; ++i)
		col += .25*texture(shade,ps+vec3((float(i/2)-.5)/256.,.008,(float(i%2)-.5)/256.)).xyz;
}
