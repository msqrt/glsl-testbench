
#version 450

uniform sampler2D tex;
in vec2 uv;

out vec4 col;

void main() {

	col = texture(tex, uv);
}
