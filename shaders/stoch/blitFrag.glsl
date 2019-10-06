
#version 450

uniform sampler2D tex;

in vec2 uv;
out vec3 color;

void main() {
	color = vec3(floatBitsToUint(texture(tex, uv*2.).xxx));
}
