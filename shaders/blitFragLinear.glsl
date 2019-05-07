
#version 450

layout(std430) buffer points {vec2 coord[];};
layout(std430) buffer cols {vec4 col[];};

uniform vec2 screenCoord;

in vec2 uv;

out vec3 color;

float roots[16]; int rootCount;

void addRoot(float t) {
	if(t<=.0 || t>=1. || isnan(t)) return;
	for(int i = 0; i<rootCount; ++i)
		if(roots[i]>t) {
			float tmp = roots[i];
			roots[i] = t;
			t = tmp;
		}
	roots[rootCount] = t;
	rootCount++;
}

void main() {
	
	color = vec3(.1);
	
	vec3 fontColor = vec3(1.);
	float area = .0;
	vec2 pixelCoord = vec2(gl_FragCoord.x, screenCoord.y-gl_FragCoord.y)-vec2(.5);

	vec2 point[4];
	for(int i = 0; i<col.length(); i++) {
		vec3 newColor = col[i].xyz;
		if(newColor!=fontColor) {
			color = mix(color, fontColor, area);
			fontColor = newColor;
			area = .0;
		}

		for(int j = 0; j<4; ++j)
			point[j] = coord[i*4+j]-pixelCoord;
		
		rootCount = 1;
		roots[0] = .0;
		if(point[0].x!=point[1].x) {
			addRoot(point[0].x/(point[0].x-point[1].x));
			addRoot((point[0].x-1.)/(point[0].x-point[1].x));
		} else continue; // vertical

		if(point[0].y!=point[1].y) {
			addRoot(point[0].y/(point[0].y-point[1].y));
			addRoot((point[0].y-1.)/(point[0].y-point[1].y));
		}

		roots[rootCount] = 1.;

		for(int j = 0; j<rootCount; ++j) {
			vec2 p = mix(point[0], point[1], .5*(roots[j]+roots[j+1]));
			if(p.x<=.0 || p.x>=1. || p.y<=.0) continue;
			vec2 p0 = mix(point[0], point[1], roots[j]), p1 = mix(point[0], point[1], roots[j+1]);
			if(p.y>=1.) {
				area += p0.x-p1.x;
			} else
				area += (p0.x-p1.x)*.5*(p0.y+p1.y);
		}
	}
	color = mix(color, fontColor, area);
}
