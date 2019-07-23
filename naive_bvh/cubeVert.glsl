
#version 450

layout(std430) buffer points{float pos[];};
layout(std430) buffer extents{float ext[];};
layout(std430) buffer buildExtents{uvec2 buildExt[];};
layout(std430) buffer buildParents{int parent[];};
uniform int N;

uniform mat4 toWorld, toClip;

float unsortable(uint x) {
	return uintBitsToFloat(x ^ (((x>>31)-1)|0x80000000));
}
vec2 unsortable(uvec2 x) {
	return vec2(unsortable(x.x), unsortable(x.y));
}
uniform int mode;
void main() {
	const int cube = gl_VertexID/24;
	vec3 p, e;
	for(int i = 0; i<3; ++i) {
		if(cube<N) {
			p[i] = pos[cube+i*N];
				p[i] += 1e17;
			e[i] = ext[cube+i*N];
		} else {
			vec2 b = unsortable(buildExt[(cube%N)*3+i]);
			p[i] = .5*(b.x+b.y);
			e[i] = .5*abs(b.y-b.x);
			if(cube>N+1024-1)// && parent[cube%N]<0)
				p[i] += 1e17;
		}
	}
	/*vec3 n = vec3(.0); float o, maxExt = .0; vec3 id = vec3(1., .0, .0);
	for(int i = 0; i<3; ++i) {
		vec2 interval = unsortable(buildExt[i]);
		if(interval.y-interval.x>maxExt) {
			maxExt = interval.y-interval.x;
			n = id;
			o = .5*(interval.x+interval.y);
		}
		id = id.zxy;
	}
	if(dot(n, p)-o>.0) p.x += 1e17;*/

	int shift = (gl_VertexID%24)/8, dir = gl_VertexID%8;
	for(int i = 0; i<3; ++i)
		p[i] += e[i]*(((dir & ((9<<i)>>shift))>0)?1.:-1.);
	gl_Position = toClip * inverse(toWorld) * vec4(p, 1.);
}
