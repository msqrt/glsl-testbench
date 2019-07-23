
#version 450

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in float weight[3];
in vec3 uv[3];
in int feature[3];

out float g_weight;
out vec3 g_uv;

void main() {
	if(weight[0]>.0)
	for(int i = 0; i<3; ++i) {
		g_weight = weight[i];
		g_uv = uv[i];
		gl_Layer = feature[i];
		gl_Position = gl_in[i].gl_Position;
		EmitVertex();
	}
	EndPrimitive();
}
