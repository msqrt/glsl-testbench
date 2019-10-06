
#version 450

out vec3 color;
in vec2 uv;

uniform float t;

uniform mat4 clipToCam, camToWorld, camToWorldOld;

vec3 trace_triangle(vec3 orig, vec3 dir, vec3 t1, vec3 t2, vec3 t3) {
	vec3 e1 = t2-t1, e2 = t3-t1;
	vec3 n = cross(e1, e2);
	mat3 m = inverse(mat3(e1, e2, n));
	
	orig = m * (orig - t1);
	dir = m * dir;
	
	float t = orig.z/dir.z;
	vec3 uvt = vec3(orig.xy-t*dir.xy, t);

	if(uvt.z<.0||uvt.x<.0||uvt.y<.0||uvt.x+uvt.y>=1.)
		uvt.z = -1.;
	
	return uvt;
}

void main() {

	float tt = .0;
	ivec2 frag = ivec2(gl_FragCoord.xy);
	float scale = 1.;
	for(int i = 0; i<int(log2(2560)); ++i) {
		ivec2 local = frag%2;
		int ind = local.x+2*local.y;
		scale *= .25;
		switch(ind) {
			case 1: tt += scale*3; break;
			case 2: tt += scale*2; break;
			case 3: tt += scale; break;
		}
		frag /= 2;
	}

	vec4 o = vec4(uv*4.-vec2(1.), -1.0, 1.), e = vec4(uv*4.-vec2(1.), 1., 1.);
	o = clipToCam*o; o /= o.w;
	e = clipToCam*e; e /= e.w;
	
	e = o-e;

	o = mix(camToWorld*o, camToWorldOld*o, tt);
	e = mix(camToWorld*e, camToWorldOld*e, tt);

	vec3 orig = o.xyz, dir = e.xyz;

	float t_plane = orig.y/dir.y;
	vec2 coord = orig.xz-dir.xz*t_plane;
	if(t_plane<.0) color = vec3(.0);
	else color = vec3(.1);

	float ang = (t+tt*.05)*3.;

	vec3 uvt = trace_triangle(orig, dir, vec3(.0, 2., .0), vec3(cos(ang), .5, sin(ang)), -vec3(cos(ang), -.5, sin(ang)));
	if(uvt.z>=.0 && (uvt.z<t_plane || t_plane<.0)) {
		t_plane = uvt.z;
		color = vec3(.0, .0, 1.);
	}
}
