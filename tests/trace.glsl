
#version 450

layout(local_size_x = 16, local_size_y = 16) in;

mat3 basis(vec3 n) {
    float sg = uintBitsToFloat(floatBitsToUint(1.)|((1<<31)&floatBitsToUint(n.z)));
    float a = -1.0 / (sg + n.z);
    float b = n.x * n.y * a;
    return mat3(vec3(1.0 + sg * n.x * n.x * a, sg * b, -sg * n.x), vec3(b, sg + n.y * n.y * a, -n.y), n);
}

uvec3 mix(uvec3 seed) {
  seed.x -= seed.y; seed.x -= seed.z; seed.x ^= (seed.z>>13);
  seed.y -= seed.z; seed.y -= seed.x; seed.y ^= (seed.x<<8); 
  seed.z -= seed.x; seed.z -= seed.y; seed.z ^= (seed.y>>13);
  seed.x -= seed.y; seed.x -= seed.z; seed.x ^= (seed.z>>12);
  seed.y -= seed.z; seed.y -= seed.x; seed.y ^= (seed.x<<16);
  seed.z -= seed.x; seed.z -= seed.y; seed.z ^= (seed.y>>5); 
  seed.x -= seed.y; seed.x -= seed.z; seed.x ^= (seed.z>>3); 
  seed.y -= seed.z; seed.y -= seed.x; seed.y ^= (seed.x<<10);
  seed.z -= seed.x; seed.z -= seed.y; seed.z ^= (seed.y>>15);
  return seed;
}

uniform int frame;
uniform int scene;

uvec3 rndSeed;
uint seedCounter;
void srnd(uvec3 seed) {
	rndSeed = mix(mix(seed));
	seedCounter = 0;
}
float rnd() {
	if(((seedCounter++) % 3) == 0) rndSeed = mix(rndSeed);
	return float(double((rndSeed = rndSeed.yzx).x)*double(pow(.5, 32.)));
}

layout(r16f) uniform image2DArray result;

vec3 fold(vec3 p, vec3 n, vec3 o) {
	return p - 2.*min(0., dot(p - o, n))*n / dot(n, n);
}

int n = 10;
float sdf(vec3 p, float t) {
	const float sc = 2.0;
	vec3 z = p * sc;
	float cc = cos(t*.05), ss = -sin(t*.05);
	z.xz = z.xz * mat2(cc, ss, -ss, cc);
	const float s = 2.;
	for (int i = 0; i<n; ++i) {
		z = fold(z, vec3(1.0, 1.0, .0), vec3(.0, .0, .0));
		z = fold(z, vec3(1.0, .0, 1.0), vec3(.0, .5, .0));
		z = fold(z, vec3(.0, 1.0, 1.0), vec3(.0, .0, .0));
		z = s * z - vec3(1.);
		z.xz = z.xz * mat2(cc, ss, -ss, cc);
	}
	float cyl = max(max(length(z.xz - vec2(.5)) - .5, z.y - .5 - .15), .5 - z.y - .15);
	return (1.0 / sc)*(length(z)-.5) * pow(s, float(-n))-.002;
}

vec3 grad(vec3 p, float t) {
	vec2 e = vec2(1.e-4, .0);
	return vec3(sdf(p + e.xyy, t) - sdf(p - e.xyy, t), sdf(p + e.yxy, t) - sdf(p - e.yxy, t), sdf(p + e.yyx, t) - sdf(p - e.yyx, t)) / (2.0*e.x);
}

uniform int renderAux;

void main() {
	ivec3 size = imageSize(result);
	if(gl_GlobalInvocationID.x<size.x && gl_GlobalInvocationID.y<size.y) {
		srnd(uvec3(1,2,scene));
		n = int(6.+12.*rnd());
		float frame_t = 200.*rnd();
		float ang = rnd()*2.*3.141592;
		float speed = max(rnd()-.5, .0)*.2;
		float shutter = 4.*max(rnd()-.5, .0);
		vec3 envCol[4];
		for(int i = 0; i<4; ++i)
			envCol[i] = vec3(rnd(), rnd(), rnd());
		vec3 o = vec3(rnd()*2.-1., rnd()*2.-1., rnd()*2.-1.)*.8;
		mat3 B = basis(-normalize(normalize(grad(o, frame_t))));
		float dof = max(.0, rnd()-.5)*.1, focal = .1+rnd()*1.8;

		srnd(uvec3(gl_GlobalInvocationID.xy, frame));

		float shutter_t = shutter*(rnd()-.5);
		float t = frame_t + shutter_t;

		o += B*vec3(cos(ang)*speed*shutter_t, sin(ang)*speed*shutter_t, .0);
		vec3 d = vec3((vec2(gl_GlobalInvocationID.xy)+vec2(rnd(), rnd()))/vec2(size)-.5, 1.);
		d = o + (B*(d*focal/d.z));
		ang = rnd()*2.*3.141592;
		o += B*vec3(cos(ang)*dof, sin(ang)*dof, .0);
		d = normalize(d-o);
		float dist = .0, g;
		for(int i = 0; i<80; ++i) {
			dist += g = sdf(o, t);
			o += g*d;
		}

		int k = 0; vec3 throughput = vec3(1.);
		while(g<.01 && k++<4) {
			float a = 2.*3.141592*rnd(), r = sqrt(rnd());
			vec3 n = normalize(grad(o, t));
			d = basis(n)*vec3(cos(a)*r, sin(a)*r, sqrt(1.-r*r));
			if(k==1 && renderAux == 1)
				for(int i = 0; i<3; ++i) {
					imageStore(result, ivec3(gl_GlobalInvocationID.xy, i+3), vec4(n[i]));
					imageStore(result, ivec3(gl_GlobalInvocationID.xy, i+6), vec4(d[i]));
				}
			o += n*.006;
			for(int i = 0; i<80; ++i) {
				g = sdf(o, t);
				o += g*d;
			}
			throughput *= .8;
		}
		if(k==0 && renderAux == 1)
			for(int i = 0; i<3; ++i) {
				imageStore(result, ivec3(gl_GlobalInvocationID.xy, i+3), vec4(.0));
				imageStore(result, ivec3(gl_GlobalInvocationID.xy, i+6), vec4(.0));
			}
		vec3 col = vec3(.0);
		if(g>.01 || isnan(g)) {
			vec3 p = normalize(d);
			vec3 env = envCol[0];
			for(int i = 1; i<4; ++i)
				env += 2.*p[i-1]*(envCol[i]-vec3(.5));
			col = max(vec3(.0), env)*vec3(throughput);
		}
		for(int i = 0; i<3; ++i)
			imageStore(result, ivec3(gl_GlobalInvocationID.xy, i), vec4(col[i]));
		if(renderAux == 1) {
			if(k<4)
				dist = 1e10;
			imageStore(result, ivec3(gl_GlobalInvocationID.xy, 9), vec4(dist*.01));
		}
	}
}
