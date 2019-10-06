
#version 460

out vec2 uv;
void main() {
	uv = vec2(gl_VertexID == 1 ? 4. : -1., gl_VertexID == 2 ? 4. : -1.);
	gl_Position = vec4(uv, -.5, 1.);
}
