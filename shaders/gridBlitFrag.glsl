
#version 450

uniform sampler2D oldPressure, density;
uniform sampler2D divergence;
uniform sampler2D velocity;
uniform sampler2D boundaryVelocity, fluidVolume, fluidRelVol;
uniform sampler2D aux;
in vec2 uv;
out vec3 color;

void main() {
	if(uv.x<.0&&uv.y>.0) {
		float w = texture(oldPressure, uv+vec2(1., .0)).x*10.;
		if(w<.0) color += -w*.1*vec3(.6, .7, 1.);
		else color += w*.1*vec3(1., .8, .7);
		//vec2 vel = texture(velocity, uv+vec2(1.,.0)).xy;
		//vel /= length(vel)+1.;
		//color = vec3(vel+vec2(1.),.0)*.5;
		color = texture(density, (uv+vec2(1.,.0))).zzz*.04;
		//color = abs(texture(fluidRelVol, uv+vec2(1., .0)).xyz)*.1;

		//color = vec3(.0);
		//if(texture(density, uv+vec2(1., .0)).z>.0)
		//	color.x += 1.;
		//if(texture(fluidVolume,uv+vec2(1.,.0)).z==.0)
		//	color.y += 1.;
	}
	if(uv.x<.0&&uv.y<.0) {
		//color = vec3(.05, abs(dFdx(texture(density, uv+vec2(1.,1.)).z)), abs(dFdy(texture(density, uv+vec2(1.,1.)).z)));
		color = 1.e5*abs(texture(divergence, uv+vec2(1.)).xxx);//*vec3(texture(velocity, uv+vec2(1.)).xy,1.);
		vec2 vel = texture(velocity, uv+vec2(1.)).xy;
		vel /= length(vel)+.1;
		color = vec3(vel+vec2(1.),.0)*.5;
	}
	if(uv.x>.0&&uv.y>.0) {
		ivec2 vsize = textureSize(velocity,0);
		ivec2 coord = ivec2(vec2(vsize)*uv);
		const float dx = 1./float(vsize.x);
		float w = (texelFetch(velocity,coord+ivec2(0,1),0).y-texelFetch(velocity,coord,0).y
		+texelFetch(velocity,coord+ivec2(1,0),0).x-texelFetch(velocity,coord,0).x)/dx;
		w *= texture(density, uv+vec2(1.,1.)).z;
		if(w<.0) color += -w*.1*vec3(.6, .7, 1.);
		else color += w*.1*vec3(1., .8, .7);
	}
	if(uv.x>.0&&uv.y<.0) {
		ivec2 vsize = textureSize(velocity,0);
		ivec2 coord = ivec2(vec2(vsize)*uv);
		const float dx = 1./float(vsize.x);
		float w = (texelFetch(velocity,coord,0).y-texelFetch(velocity,coord-ivec2(1,0),0).y
		-texelFetch(velocity,coord,0).x+texelFetch(velocity,coord-ivec2(0,1),0).x)/dx;
		color = vec3(.0);
		w *= texture(density, uv+vec2(1.,1.)).z;
		if(w<.0) color += -w*.1*vec3(.6, .7, 1.);
		else color += w*.1*vec3(1., .8, .7);
	}
}
