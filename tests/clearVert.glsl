
#version 450

out int layer;
void main() {
	layer = gl_VertexID;
}
