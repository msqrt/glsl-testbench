
#version 460
layout(std430) buffer; // set as default

buffer positionBuffer{ vec4 position[]; };
buffer uvBuffer{ vec2 uv_coords[]; };

uniform mat4 camToClip;
uniform float t;

out vec3 world_position;
out vec2 uv_coord;

void main() {
	vec4 p = position[gl_VertexID];
	p.xyz *= .01;
	world_position = p.xyz;
	p.z += .0;
	p.x -= .0;
	float c = cos(t*.5), s = sin(t*.5);
	p.xz = mat2(c, s, -s, c) * p.xz;
	p.y -= 0.05;
	p.z -= .2;

	uv_coord = uv_coords[gl_VertexID];
	gl_Position = camToClip * p;
}
