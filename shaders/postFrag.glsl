
#version 450

uniform sampler2D shade;
in vec2 uv;
out vec3 color;

void main() {
	float w_c = .0;
	color = vec3(.0);
	for(int i = 0; i<9; ++i) 
	for(int j = 0; j<9; ++j) {
		vec3 col = texture(shade, uv*.5+vec2(.5)+4.1471*vec2(i-4,j-4)/vec2(1920.,1080.)).xyz;
		if(length(col)<1. && (i!=4||j!=4)) continue;
		float w = 1./(.01+length(vec2(i-4, j-4)));
		if(i==4 && j == 4) w+=3.;
		w_c += w;
		color += w*col;
	}
	color /= w_c;
}
