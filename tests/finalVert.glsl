
#version 450

void main() {
	gl_Position = vec4((gl_VertexID==1)?3.:-1., (gl_VertexID==2)?3.:-1., .0, 1.);
}
