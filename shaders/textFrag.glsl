
#version 450

layout(std430) buffer points {vec2 coord[];};
layout(std430) buffer cols {vec4 col[];};
layout(std430) buffer inds {ivec4 ind[];};

uniform vec2 screenSize;

const int maxRootCount = 6;
float roots[maxRootCount]; int rootCount;

void addRoot(float t) {
	for(int i = 0; i<maxRootCount; ++i)
		if(i==rootCount) {
			roots[i] = t;
			break;
		} else if(roots[i]>t) {
			float tmp = roots[i];
			roots[i] = t;
			t = tmp;
		}
	rootCount++;
}

const mat4 bez = mat4(1., .0, .0, .0, -3., 3., .0, .0, 3., -6., 3., .0, -1., 3., -3., 1.);

vec2 cube(vec2 a, vec2 b, vec2 c, vec2 d, float t) {
	return d+t*(c+t*(b+t*a));
}

float cube(float a, float b, float c, float d, float t) {
	return d+t*(c+t*(b+t*a));
}

void iterativeSolve(float a, float b, float c, float d, float t0, float t1) {
	if(cube(a,b,c,d,t0)*cube(a,b,c,d,t1)<=.0) {
		float t = .5*(t0+t1);
		for(int i = 0; i<4; ++i)
			t = clamp(t-(d+t*(c+t*(b+t*a)))/(c+t*(2.*b+3.*t*a)), t0, t1);
		addRoot(t);
	}
}

const float pi = 3.14159265358979323;
void solveCubic(float a, float b, float c, float d) {
	float dA = 3.*a, dB = 2.*b, dC = c, t0 = -1., t1 = -1.;
	if(abs(dA)>1.e-6) {
		float dD = dB*dB-4.*dA*dC;
		if(dD>=.0) {
			dA = -.5/dA;
			t0 = dB*dA - abs(dA*sqrt(dD));
			t1 = dB*dA + abs(dA*sqrt(dD));
		}
	}else t0 = t1 = -dC/dB;
	float prev = .0;
	if(t0>prev && t0<1.) {
		iterativeSolve(a,b,c,d,.0,t0);
		iterativeSolve(a,b,c,d-1.,.0,t0);
		prev = t0;
	}
	if(t1>prev && t1<1.) {
		iterativeSolve(a,b,c,d,prev,t1);
		iterativeSolve(a,b,c,d-1.,prev,t1);
		prev = t1;
	}
	if(prev<1.) {
		iterativeSolve(a,b,c,d,prev,1.);
		iterativeSolve(a,b,c,d-1.,prev,1.);
	}
}

float integrateBezier(vec2 a, vec2 b, vec2 c, vec2 d, float t0, float t1) {
	a.x *= 3.; b.x *= 2.;
	float A[6];
	A[5] = a.x*a.y;
	A[4] = a.x*b.y+b.x*a.y;
	A[3] = a.x*c.y+b.x*b.y+c.x*a.y;
	A[2] = a.x*d.y+b.x*c.y+c.x*b.y;
	A[1] = b.x*d.y+c.x*c.y;
	A[0] = c.x*d.y;
	float I = .0;
	vec2 acc = vec2(1.);
	for(int i = 0; i<6; ++i) {
		acc *= vec2(t0, t1);
		I += A[i]*(acc.x-acc.y)/float(i+1);
	}
	return I;
}

in vec4 boundingBox;
in flat ivec2 range;

uniform vec3 textColor;
out vec4 color;

void main() {
	color.xyz = vec3(.0);
	vec3 fontColor = textColor;
	float area = .0;

	vec2 pixelCoord = vec2(gl_FragCoord.x, screenSize.y-gl_FragCoord.y)-vec2(.5)-boundingBox.xy;

	for(int i = range.x; i<range.y; i++) {
		vec3 newColor = col[i].xyz;
		if(newColor.x<.0)
			newColor = textColor;
		if(newColor!=fontColor) {
			color.xyz = mix(color.xyz, fontColor, area);
			color.a = max(color.a, area);
			fontColor = newColor;
			area = .0;
		}
		ivec4 index = ind[i];
		for(int k = index.x; k<index.y; k+= 4) {
			vec2 point[4];
			float minC = 2.;
			vec2 maxC = vec2(-1.);
			for(int j = 0; j<4; ++j) {
				point[j] = coord[k+j]-pixelCoord;
				minC = min(minC, point[j].x);
				maxC = max(maxC, point[j]);
			}
			if(minC>=1.0 || maxC.x<=.0 || maxC.y<=.0) continue;

			rootCount = 1;
			roots[0] = .0;

			mat4x2 a = mat4x2(point[0], point[1], point[2], point[3]) * bez;
			solveCubic(a[3].x, a[2].x, a[1].x, a[0].x);
			solveCubic(a[3].y, a[2].y, a[1].y, a[0].y);

			roots[rootCount] = 1.;

			for(int j = 0; j<rootCount; ++j) {
				vec2 p = cube(a[3], a[2], a[1], a[0],.5*(roots[j]+roots[j+1]));
				if(p.x<=.0 || p.x>=1. || p.y<=.0) continue;
				if(p.y>=1.)
					area += cube(a[3].x, a[2].x, a[1].x, a[0].x,roots[j])-cube(a[3].x, a[2].x, a[1].x, a[0].x,roots[j+1]);
				else
					area += integrateBezier(a[3], a[2], a[1], a[0], roots[j], roots[j+1]);
			}
		}
		// TODO: quadratic?
		for(int k = index.z; k<index.w; k+= 2) {
			vec2 point[2];
			float minC = 2.;
			vec2 maxC = vec2(-1.);
			for(int j = 0; j<2; ++j) {
				point[j] = coord[k+j]-pixelCoord;
				minC = min(minC, point[j].x);
				maxC = max(maxC, point[j]);
			}
			if(minC>=1.0 || maxC.x<=.0 || maxC.y<=.0) continue;
			
			rootCount = 1;
			roots[0] = .0;
			if(point[0].x!=point[1].x) {
				float t = point[0].x/(point[0].x-point[1].x);
				if(t>.0 && t<1.)addRoot(t);
				t = (point[0].x-1.)/(point[0].x-point[1].x);
				if(t>.0 && t<1.)addRoot(t);
			}

			if(point[0].y!=point[1].y) {
				float t = point[0].y/(point[0].y-point[1].y);
				if(t>.0 && t<1.)addRoot(t);
				t = (point[0].y-1.)/(point[0].y-point[1].y);
				if(t>.0 && t<1.)addRoot(t);
			}

			roots[rootCount] = 1.;

			for(int j = 0; j<rootCount; ++j) {
				vec2 p = mix(point[0], point[1], .5*(roots[j]+roots[j+1]));
				if(p.x<=.0 || p.x>=1. || p.y<=.0) continue;
				vec2 p0 = mix(point[0], point[1], roots[j]), p1 = mix(point[0], point[1], roots[j+1]);
				if(p.y>=1.)
					area += p0.x-p1.x;
				else
					area += (p0.x-p1.x)*.5*(p0.y+p1.y);
			}
		}
	}
	color.xyz = mix(color.xyz, fontColor, area);
	color.a = max(color.a, area);
	if(color.a>.0) color.xyz /= color.a;
	color.xyz = pow(color.xyz, vec3(3.2));
	color.a = pow(color.a, 1.5);
}
