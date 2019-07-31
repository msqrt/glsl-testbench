
#version 450

layout(points) in;
layout(triangle_strip, max_vertices=4) out;

layout(std430) buffer posBuffer {vec4 particle_pos[];};
layout(std430) buffer velBuffer {vec4 particle_vel[];};

out vec2 uv;
out vec3 ps;

uniform int scene;
uniform float t;

mat4 lookat(vec3 o,vec3 t,vec3 u) {
	mat4 r;
	r[2]=vec4(-normalize(t-o),.0);
	r[0]=vec4(normalize(cross(r[2].xyz,u)),.0);
	r[1]=vec4(cross(r[0].xyz,r[2].xyz),.0);
	r[3]=vec4(o,1.);
	return inverse(r);
}

mat4 proj(float fov, float w_over_h, float near, float far) {
	mat4 r = mat4(.0);
	r[0][0] = 1./(w_over_h*atan(fov*.5));
	r[1][1] = 1./atan(fov*.5);
	r[2][2] = -(far+near)/(far-near);
	r[2][3] = -1.;
	r[3][2] = -2.*far*near/(far-near);
	return r;
}

void main() {
	
	vec4 pos = vec4(particle_pos[gl_PrimitiveIDIn].xyz, 1.);
	vec4 velocity = particle_vel[gl_PrimitiveIDIn];

	vec3 orig, target, up = vec3(.0,1.,.0);
	switch(scene) {
		case 0:
		orig = vec3(.5,.6,.5)+vec3(cos(t+.5), -t*.01, sin(t+.5))*(.5+t*.025);
		target = vec3(.5,.2,.5);
		break;
		case 1:
		orig = vec3(.5)+vec3(cos(t*.5)*.5, -.4, sin(t*.5)*.5);
		target = vec3(.5,.57,.5);
		break;
		case 2:
		orig = vec3(.5)-vec3(.0,-.1+t*.01,1.5);
		target = vec3(.5)-vec3(.0,t*.01+.1,.0);
		break;
		case 3:
		orig = vec3(.5)+vec3(.6, .05, t*.015-.1);
		target = vec3(.5)+vec3(.0, -.05, t*.01-.1);
		break;
		case 4:
		orig = vec3(.5)+vec3(t*.015-.2, -.15, -1.1);
		target = vec3(.5)+vec3(-t*.01-.1, -.25, .0);
		break;
		case 5:
		orig = vec3(.5,-.3,.5)+.6*vec3(cos(t), .0, sin(t));
		target = vec3(.5)+vec3(.0, -.3+t*.02, .0);
		break;
		case 6:
		orig = vec3(.5)+vec3(t*.015-.1, .05, .6);
		target = vec3(.5)+vec3(t*.01-.1, -.05, .0);
		break;
		case 7:
		orig = vec3(.5)+vec3(.8, -.35+t*.04, .0);
		target = vec3(.5)+vec3(.0, -.35+t*.005, .0);
		break;
		case 8:
		orig = vec3(.5)+vec3(-.1+t*.01, 1., .0);
		target = vec3(.5);
		up = vec3(.0, .0, 1.);
		break;
	}

	vec4 cpos = lookat(orig,target,up) * pos;

	for(int i = 0; i<4; ++i) {
		uv = vec2(float(i%2), float(i/2));
		vec4 p = cpos;
		ps = pos.xyz;
		p.xy += (uv-vec2(.5))*.005;
		gl_Position = proj(.65, 16./9., .01, 5.) * p;
		EmitVertex();
	}
	EndPrimitive();
}
