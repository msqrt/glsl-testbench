
#version 450

out vec2 uv;

void main() {
	uv = vec2(gl_VertexID==1?2.:.0, gl_VertexID==2?2.:.0);
	gl_Position = vec4(uv*2-vec2(1.), .0, 1.);
}