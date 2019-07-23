
#version 450

layout(r16f) uniform image2DArray noisy;
layout(r16f) uniform image2DArray result;
layout(r16f) uniform image2DArray target;
out vec3 color;

const mat3 opponent = mat3(1./3., 1./sqrt(6.), 1./(3.*sqrt(2.)),1./3., .0, -sqrt(2.)/3., 1./3., -1./sqrt(6.), 1./(3.*sqrt(2.)));

void main() {
	if(gl_FragCoord.x<512) {
		int base = int((gl_FragCoord.x*4)/512);
		for(int i = 0; i<3; ++i)
			color[i] = imageLoad(noisy, ivec3(gl_FragCoord.xy, base*3+i)).x;
	}
	else if(gl_FragCoord.x>=1024)
		for(int i = 0; i<3; ++i)
			color[i] = imageLoad(target, ivec3(gl_FragCoord.xy, i)-ivec3(1024, 0, 0)).x;
	else {
		for(int i = 0; i<3; ++i)
			color[i] = imageLoad(result, ivec3(gl_FragCoord.xy, i)-ivec3(512, 0, 0)).x;
		color = opponent*color;
	}
}
