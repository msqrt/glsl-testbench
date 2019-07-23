
#version 450

layout(std430) buffer points {vec4 point[];};
layout(std430) buffer faces {ivec4 face[];};

uniform mat4 toClip;
uniform float t;

out vec3 p[4];
out vec3 ray;
out flat int quad;

void main() {
	quad = gl_VertexID/12;
	int corner = (((gl_VertexID%2)==0)?1:0)+(((((gl_VertexID+1)/3)%2)==0)?2:0);
	if((gl_VertexID%12)<6)
		corner = (corner+2)%4;

	vec2 c = vec2(cos(t), sin(t));
	mat2 m = mat2(c.x, c.y, -c.y, c.x);
	ivec4 q = face[quad*3];
	for(int i = 0; i<4; ++i) {
		p[i] = point[q[i]].xyz;
		p[i] *= .02;
		p[i].xz *= m;
		p[i].z -= .8;
		if(i == corner) ray = p[i];
	}
	gl_Position = toClip * vec4(ray, 1.);
}
