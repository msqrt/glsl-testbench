
#version 450

uniform sampler2D oldPressure;
uniform sampler2D divergence;
uniform sampler2D result;
uniform sampler2D velocity;
uniform sampler2D boundaryVelocity, fluidVolume;
uniform sampler2D aux;
in vec2 uv;
out vec3 color;

void main() {
	if(uv.x<.0&&uv.y<.0)
		color = texture(result, uv+vec2(1.)).xyz + vec3(1.-2.*abs(texture(fluidVolume, uv).x-.5));
	if(uv.x>.0&&uv.y<.0) {
		ivec2 coord = ivec2(512.*(uv+vec2(.0,1.)));
		const float dx = 1./512.;
		float w = (texelFetch(velocity,coord,0).y-texelFetch(velocity,coord-ivec2(1,0),0).y
		-texelFetch(velocity,coord,0).x+texelFetch(velocity,coord-ivec2(0,1),0).x)/dx;
		color = vec3(.0);
		if(w<.0) color += -w*.01*vec3(.6, .7, 1.);
		else color += w*.01*vec3(1., .8, .7);
	}
	if(uv.x<.0&&uv.y>.0) {
		//color = 1e2*texture(divergence, uv+vec2(1.,.0)).xyz;
		color = abs(texture(boundaryVelocity, uv+vec2(1.,.0)).xyz)
			*(1.-2.*abs(texture(fluidVolume, uv).x-.5));
		//color = vec3(1.-2.*abs(texture(fluidVolume, uv).x-.5));
	}
	if(uv.x>.0&&uv.y>.0) {
		ivec2 coord = ivec2(512.*uv);
		const float dx = 1./512.;
		float w = (texelFetch(velocity,coord+ivec2(0,1),0).y-texelFetch(velocity,coord,0).y
		+texelFetch(velocity,coord+ivec2(1,0),0).x-texelFetch(velocity,coord,0).x)/dx;
		if(w<.0) color += -w*.1*vec3(.6, .7, 1.);
		else color += w*.1*vec3(1., .8, .7);
	}
}
