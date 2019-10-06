
#version 460
layout(std430) buffer; // set as default

buffer positionBuffer{ vec4 position[]; };

uniform mat4 camToClip;
uniform float t;

out vec3 world_position;

void main() {
	vec4 p = position[gl_VertexID];
	p.xyz *= .001;
	world_position = p.xyz;
	p.z += .3;
	p.x -= .3;
	float c = cos(t*.5), s = sin(t*.5);
	p.xz = mat2(c, s, -s, c) * p.xz;
	p.y -= .3;
	p.z -= .5;

	gl_Position = camToClip * p;
}
