
#version 450

layout(local_size_x = 256) in;

layout(rgba32f) uniform image3D fluidVolume, densityImage;
layout(rgba16f) uniform image3D result;
layout(rg32f) uniform image3D velocity, velDifference, boundaryVelocity;
layout(r32f) uniform image3D pressure, divergenceImage;
uniform sampler3D oldVelocity, oldDensity, oldPressure, divergence, fluidVol, velDiff;

layout(std430) buffer posBuffer {vec4 particle_pos[];};
layout(std430) buffer velBuffer {vec4 particle_vel[];};
layout(std430) buffer affBuffer {vec4 affine[];};

uniform float t, dt, dx;

uniform int mode, size;

float eval_dens(vec3 pos) {
	return texture(oldDensity, pos).w;
}

float bilin_kernel(vec3 p) {
	return max(1.-abs(p.x),.0)*max(1.-abs(p.y),.0)*max(1.-abs(p.z),.0);
}
vec3 bilin_kernel_grad(vec3 p) {
	return -sign(p)*vec3(max(1.-abs(p.y),.0)*max(1.-abs(p.z),.0), max(1.-abs(p.x),.0)*max(1.-abs(p.z),.0), max(1.-abs(p.x),.0)*max(1.-abs(p.y),.0));
}


vec3 eval_vel(vec3 pos) {
	vec3 vel = vec3(.0);
	
	//vec2 frac_x = pos*vec2(size)-vec2(.5, 1.);
	vec3 frac_x = pos*vec3(size)-vec3(.0, .5, .5);
	for(int i = 0; i<2; ++i)
		for(int j = 0; j<2; ++j)
			for(int k = 0; k<2; ++k)
				vel.x += bilin_kernel(frac_x-floor(frac_x)-vec3(i,j,k))*texelFetch(oldVelocity, ivec3(frac_x)+ivec3(i,j,k),0).x;
	//vec2 frac_y = pos*vec2(size)-vec2(1., .5);
	vec3 frac_y = pos*vec3(size)-vec3(.5, .0, .5);
	for(int i = 0; i<2; ++i)
		for(int j = 0; j<2; ++j)
			for(int k = 0; k<2; ++k)
				vel.y += bilin_kernel(frac_y-floor(frac_y)-vec3(i,j,k))*texelFetch(oldVelocity, ivec3(frac_y)+ivec3(i,j,k),0).y;
	//vec2 frac_z = pos*vec2(size)-vec2(1., .5);
	vec3 frac_z = pos*vec3(size)-vec3(.5, .5, .0);
	for(int i = 0; i<2; ++i)
		for(int j = 0; j<2; ++j)
			for(int k = 0; k<2; ++k)
				vel.z += bilin_kernel(frac_z-floor(frac_z)-vec3(i,j,k))*texelFetch(oldVelocity, ivec3(frac_z)+ivec3(i,j,k),0).z;

	return vel;
}


mat3 eval_affine(vec3 pos) {
	mat3 res = mat3(.0);
	
	//vec2 frac_x = pos*vec2(size)-vec2(.5, 1.);
	vec3 frac_x = pos*vec3(size)-vec3(.0, .5, .5);
	for(int i = 0; i<2; ++i)
		for(int j = 0; j<2; ++j)
			for(int k = 0; k<2; ++k)
				res[0] += bilin_kernel_grad(frac_x-floor(frac_x)-vec3(i,j,k))*texelFetch(oldVelocity, ivec3(frac_x)+ivec3(i,j,k),0).x;
	//vec2 frac_y = pos*vec2(size)-vec2(1., .5);
	vec3 frac_y = pos*vec3(size)-vec3(.5, .0, .5);
	for(int i = 0; i<2; ++i)
		for(int j = 0; j<2; ++j)
			for(int k = 0; k<2; ++k)
				res[1] += bilin_kernel_grad(frac_y-floor(frac_y)-vec3(i,j,k))*texelFetch(oldVelocity, ivec3(frac_y)+ivec3(i,j,k),0).y;
	//vec2 frac_y = pos*vec2(size)-vec2(1., .5);
	vec3 frac_z = pos*vec3(size)-vec3(.5, .5, .0);
	for(int i = 0; i<2; ++i)
		for(int j = 0; j<2; ++j)
			for(int k = 0; k<2; ++k)
				res[2] += bilin_kernel_grad(frac_z-floor(frac_z)-vec3(i,j,k))*texelFetch(oldVelocity, ivec3(frac_z)+ivec3(i,j,k),0).z;

	return res;
}

vec3 eval_vel_diff(vec3 pos) {
	vec3 vel = vec3(.0);
	
	//vec2 frac_x = pos*vec2(size)-vec2(.5, 1.);
	vec3 frac_x = pos*vec3(size)-vec3(.0, .5, .5);
	for(int i = 0; i<2; ++i)
		for(int j = 0; j<2; ++j)
			for(int k = 0; k<2; ++k)
				vel.x += bilin_kernel(frac_x-floor(frac_x)-vec3(i,j,k))*texelFetch(velDiff, ivec3(frac_x)+ivec3(i,j,k),0).x;
	//vec2 frac_y = pos*vec2(size)-vec2(1., .5);
	vec3 frac_y = pos*vec3(size)-vec3(.5, .0, .5);
	for(int i = 0; i<2; ++i)
		for(int j = 0; j<2; ++j)
			for(int k = 0; k<2; ++k)
				vel.y += bilin_kernel(frac_y-floor(frac_y)-vec3(i,j,k))*texelFetch(velDiff, ivec3(frac_y)+ivec3(i,j,k),0).y;
	//vec2 frac_z = pos*vec2(size)-vec2(1., .5);
	vec3 frac_z = pos*vec3(size)-vec3(.5, .5, .0);
	for(int i = 0; i<2; ++i)
		for(int j = 0; j<2; ++j)
			for(int k = 0; k<2; ++k)
				vel.z += bilin_kernel(frac_z-floor(frac_z)-vec3(i,j,k))*texelFetch(velDiff, ivec3(frac_z)+ivec3(i,j,k),0).z;

	return vel;
}


mat3 eval_affine_diff(vec3 pos) {
	mat3 res = mat3(.0);
	
	//vec2 frac_x = pos*vec2(size)-vec2(.5, 1.);
	vec3 frac_x = pos*vec3(size)-vec3(.0, .5, .5);
	for(int i = 0; i<2; ++i)
		for(int j = 0; j<2; ++j)
			for(int k = 0; k<2; ++k)
				res[0] += bilin_kernel_grad(frac_x-floor(frac_x)-vec3(i,j,k))*texelFetch(velDiff, ivec3(frac_x)+ivec3(i,j,k),0).x;
	//vec2 frac_y = pos*vec2(size)-vec2(1., .5);
	vec3 frac_y = pos*vec3(size)-vec3(.5, .0, .5);
	for(int i = 0; i<2; ++i)
		for(int j = 0; j<2; ++j)
			for(int k = 0; k<2; ++k)
				res[1] += bilin_kernel_grad(frac_y-floor(frac_y)-vec3(i,j,k))*texelFetch(velDiff, ivec3(frac_y)+ivec3(i,j,k),0).y;
	//vec2 frac_y = pos*vec2(size)-vec2(1., .5);
	vec3 frac_z = pos*vec3(size)-vec3(.5, .5, .0);
	for(int i = 0; i<2; ++i)
		for(int j = 0; j<2; ++j)
			for(int k = 0; k<2; ++k)
				res[2] += bilin_kernel_grad(frac_z-floor(frac_z)-vec3(i,j,k))*texelFetch(velDiff, ivec3(frac_z)+ivec3(i,j,k),0).z;

	return res;
}


float solidDist(vec3 uv, float eval_t) {
	uv -= vec3(.5);
	return min(min(.45-abs(uv.z), .45-abs(uv.x)), min(length(uv-vec3(-.5, -.5, -.5))-.3, .45-abs(uv.y)));
}

void main() {
	int index = int(gl_GlobalInvocationID.x);
	if(mode == 0) { // init
		int w = int(pow(float(particle_pos.length()), 1./3.));
		vec3 uv = vec3(index/(w*w), (index/w)%w, index%w)/float(w);
		particle_pos[index] = vec4(vec3(.4,.2,.4)+uv*vec3(.45,.4, .45), .0);
		particle_vel[index] = vec4(.0);
		for(int i = 0; i<3; ++i)
			affine[index*3+i] = vec4(.0);
	} else if(mode == 1) { // grid to particle + advect
		vec3 pos = particle_pos[index].xyz;

		if(pos.x<.0||pos.y<.0||pos.z<.0||pos.x>=63./64.||pos.y>=63./64.||pos.z>=63./64.) {
			vec3 vel = particle_vel[index].xyz-vec3(.0,.25*dt,.0);
			pos += dt*vel;
			particle_pos[index] = vec4(pos,.0);
			particle_vel[index] = vec4(vel,.0);
		} else {
			vec3 new_vel = eval_vel(pos);
			vec3 veld = eval_vel_diff(pos);
			mat3 m = eval_affine(pos);
			mat3 md = eval_affine_diff(pos);

			vec3 k1 = eval_vel(pos);
			vec3 k2 = eval_vel(pos+.5*dt*k1);
			vec3 k3 = eval_vel(pos+.75*dt*k2);

			pos += (2.*k1+3.*k2+4.*k3)*dt/9.;

			particle_pos[index] = vec4(pos,.0);

			//particle_vel[index] = vec4(new_vel,.0);
			//for(int i = 0; i<3; ++i)
			//	affine[index*3+i] = vec4(m[i],.0);
			particle_vel[index] = vec4(veld,.0);
			for(int i = 0; i<3; ++i)
				affine[index*3+i] = vec4(md[i],.0);
		}
	}
}
