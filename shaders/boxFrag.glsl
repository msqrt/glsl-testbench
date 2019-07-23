
#version 450

in vec2 uv;
out vec3 color;

void main() {
	color = vec3(1., .2, .02);
	if(uv.x<.05 || uv.y<.05 || uv.x>.95 || uv.y>.95)
		color *= .5;
}