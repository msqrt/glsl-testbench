
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

const vec3 spherecenters[4] = vec3[4](vec3(-.2, -.2, -.2),vec3(.28, .2, -.3),vec3(.0, .28, .0),vec3(.2, -.3, .1));

uniform int scene, frame;

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
	float box = min(min(.45-abs(uv.z), .45-abs(uv.x)), .45-abs(uv.y));
	if(scene==0) {
		return min(length(uv-vec3(-.5, -.5, -.5))-.3, box);
	}
	if(scene==1||scene==2) {
		return box;
	}
	if(scene==3) {
		return min(box,.1*(sin(uv.y*35.-uv.z*22.)+sin(uv.x*35.+uv.y*29.)+sin(uv.z*45.-uv.x*4.))+.6-5.*abs(uv.y));
	}
	if(scene==4) {
		return min(box,min(min(length(uv.xz-vec2(-.1,-.24))-.1,length(uv.xz-vec2(-.1,.24))-.1),length(uv.xz-vec2(.3, .0))-.1));
	}
	if(scene==5) {
		float s = .0;
		for(int i = 0; i<4; ++i)
			s = max(s, .2-length(uv-spherecenters[i]));
		return s;
	}
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
		
		vec3 pos;
		
		if(frame>0) {
			pos = particle_pos[index].xyz;

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
		} else { // inits
			if(scene == 0) {
				int w = int(pow(float(particle_pos.length()), 1./3.));
				vec3 uv = vec3(index/(w*w), (index/w)%w, index%w)/float(w);
				particle_pos[index] = vec4(vec3(.4,.2,.4)+uv*vec3(.45,.4, .45), .0);
				particle_vel[index] = vec4(.0);
			} else if(scene==2) {
				if(index<192*1024) {
					int w = int(pow(float(192*1024), 1./3.));
					vec3 uv = vec3(index/(w*w), (index/w)%w, index%w)/float(w);
					particle_pos[index] = vec4(vec3(.07,.07,.07)+uv*vec3(1.-.07*2.,.15, 1.-.07*2.), .0);
					particle_vel[index] = vec4(.0);
				} else {
					int vindex = index-192*1024;
					int w = int(pow(float(64*1024), 1./3.));
					vec3 uv = vec3(vindex/(w*w), (vindex/w)%w, vindex%w)/float(w);
					vec4 pos = vec4(vec3(.3,.5,.3)+uv*vec3(.4,.4, .4), .0);
					if(uv.y<1.-min(1.-2.*abs(.5-uv.x), 1.-2.*abs(.5-uv.z))) pos.x = 1e8;
					particle_pos[index] = pos;
					particle_vel[index] = .5*vec4((.5-uv.z)*.4-.2*(uv.x-.5), .0, (uv.x-.5)*.4-.2*(uv.z-.5), .0);
				}
			} else if(scene==5) {
				int sphere = index/(64*1024);
				int vindex = index%(64*1024);
				int w = int(pow(float(64*1024), 1./3.));
				vec3 uv = vec3(vindex/(w*w), (vindex/w)%w, vindex%w)/float(w);
				particle_pos[index] = vec4(spherecenters[sphere]+vec3(.42)+uv*.14, .0);
				particle_vel[index] = vec4(.0);
			}
		}
		if(scene == 1) {
			if(index>=(frame+1)*80)
				particle_pos[index] = vec4(1e8);
			else if(index>=frame*80) {
				float r = float(index-frame*80);
				float ang = 2.4*r+1.9*float(frame);
				r = sqrt(r/80.)*.01;
				vec4 pos = vec4(.92+.01*cos(ang*1924.9), .5-r*cos(ang), .5-r*sin(ang), 1.);
				vec4 vel = vec4(-.8+.1*sin(float(index)*491.), .5+.1*cos(float(index)*1999.), .0, .0)*.5;
				if((index%2)==1) {pos.x = 1.-pos.x; pos.y-=.02; vel.x = -vel.x;}
				particle_pos[index] = pos;
				particle_vel[index] = vel;
			}
		} else if(scene == 3) {
			if(index>=(frame+1)*64)
				particle_pos[index] = vec4(1e8);
			else if(index>=frame*64) {
				float r = float(index-frame*64);
				float ang = 2.4*r+1.9*float(frame);
				vec4 pos = vec4(.07, .5+.01*cos(ang*14.125), .07+(1.-.07*2.)*r/64.+cos(ang*1942.129)*.01, 1.);
				vec4 vel = vec4(.02, .0, .0, .0);
				particle_pos[index] = pos;
				particle_vel[index] = vel;
			}
		} else if(scene == 4) {
			if(index>=(frame+1)*128)
				particle_pos[index] = vec4(1e8);
			else if(index>=frame*128) {
				float r = float(index-frame*128);
				float ang = 2.4*r+1.9*float(frame);
				vec4 pos = vec4(.07, .6+.04*cos(ang*14.125), .07+(1.-.07*2.)*r/128.+cos(ang*1942.129)*.01, 1.);
				vec4 vel = vec4(.1, .0, .0, .0);
				particle_pos[index] = pos;
				particle_vel[index] = vel;
			}
		}
	}
}
