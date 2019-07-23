
#version 450

layout(points) in;
layout(triangle_strip, max_vertices = 3) out;

in int layer[1];

void main() {
	for(int i = 0; i<3; ++i) {
		gl_Layer = layer[0];
		gl_Position = vec4((i==1)?3.:-1., (i==2)?3.:-1., .0, 1.);
		EmitVertex();
	}
	EndPrimitive();
}
