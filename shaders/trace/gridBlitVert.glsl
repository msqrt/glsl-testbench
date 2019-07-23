
#version 450

void main() {
	gl_Position = vec4(gl_VertexID==1?4.:-1., gl_VertexID==2?4.:-1., .0, 1.);
}
