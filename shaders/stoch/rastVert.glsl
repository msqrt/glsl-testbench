
#version 450

out vec4 begin, end;

uniform mat4 camToClip, worldToCam, worldToCamOld;

uniform float t;

void main() {
	vec2 uv = vec2((gl_VertexID==0)?1.:.0, (gl_VertexID==1)?1.:.0);
	float ang = t*3.;
	vec3 p = (1.-uv.x-uv.y)*vec3(.0, 2., .0) + uv.x*vec3(cos(ang), .5, sin(ang)) + uv.y * vec3(-cos(ang), .5, -sin(ang));
	vec3 v = vec3(p.z, .0, -p.x)*3.*.05;
	begin = camToClip * worldToCamOld * vec4(p-.5*v, 1.);
	end = camToClip * worldToCam * vec4(p+.5*v, 1.);
	gl_Position = camToClip * (worldToCam+worldToCamOld)*.5 * vec4(p, 1.);
}
