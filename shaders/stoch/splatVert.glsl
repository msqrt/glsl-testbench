
#version 450

out vec2 uv;

void main() {
	uv = vec2((gl_VertexID==0)?1.:.0, (gl_VertexID==1)?1.:.0);
	gl_Position = vec4(vec2(-1.)+4.*uv, .0, 1.);
}
