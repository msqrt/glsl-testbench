
#version 460

layout(std430) buffer;

buffer materialIndexBuffer{ uint materialIndex[]; };
buffer materialAlbedoBuffer{ vec4 materialAlbedo[]; };

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in vec3 world_position[];
out vec3 albedo, position, normal_varying;

uniform int res;

void main() {
	vec4 pos[3];
	for (int i = 0; i < 3; ++i)
		pos[i] = gl_in[i].gl_Position;

	albedo = materialAlbedo[materialIndex[gl_PrimitiveIDIn]].xyz; // fetch albedo for triangle

	vec3 normal = normalize(cross(pos[1].xyz - pos[0].xyz, pos[2].xyz - pos[0].xyz));

	for (int i = 0; i < 3; ++i) {
		normal_varying = normal;
		position = world_position[i];
		gl_Position = pos[i];
		EmitVertex();
	}
	EndPrimitive();
}
