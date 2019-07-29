
#version 450

layout(local_size_x = 256) in;

layout(rgba32f) uniform image2D fluidVolume, densityImage;
layout(rgba16f) uniform image2D result;
layout(rg32f) uniform image2D velocity, velDifference, boundaryVelocity;
layout(r32f) uniform image2D pressure, divergenceImage;
uniform sampler2D oldVelocity, oldDensity, oldPressure, divergence, fluidVol, velDiff;

layout(std430) buffer posBuffer {vec2 particle_pos[];};
layout(std430) buffer velBuffer {vec2 particle_vel[];};
layout(std430) buffer affBuffer {vec4 affine[];};

uniform float t, dt;

uniform int mode;

float eval_dens(vec2 pos) {
	ivec2 size = textureSize(oldVelocity, 0);
	vec2 dx = 1./vec2(size);
	return texture(oldDensity, pos).z;
}

float bilin_kernel(vec2 p) {
	return max(1.-abs(p.x),.0)*max(1.-abs(p.y),.0);
}
vec2 bilin_kernel_grad(vec2 p) {
	return -sign(p)*vec2(max(1.-abs(p.y),.0), max(1.-abs(p.x),.0));
}

/*vec2 eval_vel(vec2 pos) {
	ivec2 size = textureSize(oldVelocity, 0);
	vec2 dx = 1./vec2(size);
	vec2 vel = vec2(texture(oldVelocity, pos-vec2(.0, .5)*dx).x, texture(oldVelocity, pos-vec2(.5, .0)*dx).y);
	return vel;
}*/

vec2 eval_vel(vec2 pos) {
	ivec2 size = textureSize(oldVelocity, 0);
	vec2 dx = 1./vec2(size);
	vec2 vel = vec2(.0);
	
	//vec2 frac_x = pos*vec2(size)-vec2(.5, 1.);
	vec2 frac_x = pos*vec2(size)-vec2(.0, .5);
	for(int i = 0; i<2; ++i)
		for(int j = 0; j<2; ++j)
			vel.x += bilin_kernel(frac_x-floor(frac_x)-vec2(i,j))*texelFetch(oldVelocity, ivec2(frac_x)+ivec2(i,j),0).x;
	//vec2 frac_y = pos*vec2(size)-vec2(1., .5);
	vec2 frac_y = pos*vec2(size)-vec2(.5, .0);
	for(int i = 0; i<2; ++i)
		for(int j = 0; j<2; ++j)
			vel.y += bilin_kernel(frac_y-floor(frac_y)-vec2(i,j))*texelFetch(oldVelocity, ivec2(frac_y)+ivec2(i,j),0).y;

	return vel;
}


mat2 eval_affine(vec2 pos) {
	ivec2 size = textureSize(oldVelocity, 0);
	vec2 dx = 1./vec2(size);
	mat2 res = mat2(.0);
	
	//vec2 frac_x = pos*vec2(size)-vec2(.5, 1.);
	vec2 frac_x = pos*vec2(size)-vec2(.0, .5);
	for(int i = 0; i<2; ++i)
		for(int j = 0; j<2; ++j)
			res[0] += bilin_kernel_grad(frac_x-floor(frac_x)-vec2(i,j))*texelFetch(oldVelocity, ivec2(frac_x)+ivec2(i,j),0).x;
	//vec2 frac_y = pos*vec2(size)-vec2(1., .5);
	vec2 frac_y = pos*vec2(size)-vec2(.5, .0);
	for(int i = 0; i<2; ++i)
		for(int j = 0; j<2; ++j)
			res[1] += bilin_kernel_grad(frac_y-floor(frac_y)-vec2(i,j))*texelFetch(oldVelocity, ivec2(frac_y)+ivec2(i,j),0).y;

	return res;
}

vec2 eval_vel_diff(vec2 pos) {
	ivec2 size = textureSize(oldVelocity, 0);
	vec2 dx = 1./vec2(size);
	vec2 vel = vec2(.0);
	
	//vec2 frac_x = pos*vec2(size)-vec2(.5, 1.);
	vec2 frac_x = pos*vec2(size)-vec2(.0, .5);
	for(int i = 0; i<2; ++i)
		for(int j = 0; j<2; ++j)
			vel.x += bilin_kernel(frac_x-floor(frac_x)-vec2(i,j))*texelFetch(velDiff, ivec2(frac_x)+ivec2(i,j),0).x;
	//vec2 frac_y = pos*vec2(size)-vec2(1., .5);
	vec2 frac_y = pos*vec2(size)-vec2(.5, .0);
	for(int i = 0; i<2; ++i)
		for(int j = 0; j<2; ++j)
			vel.y += bilin_kernel(frac_y-floor(frac_y)-vec2(i,j))*texelFetch(velDiff, ivec2(frac_y)+ivec2(i,j),0).y;

	return vel;
}

mat2 eval_affine_diff(vec2 pos) {
	ivec2 size = textureSize(oldVelocity, 0);
	vec2 dx = 1./vec2(size);
	mat2 res = mat2(.0);
	
	//vec2 frac_x = pos*vec2(size)-vec2(.5, 1.);
	vec2 frac_x = pos*vec2(size)-vec2(.0, .5);
	for(int i = 0; i<2; ++i)
		for(int j = 0; j<2; ++j)
			res[0] += bilin_kernel_grad(frac_x-floor(frac_x)-vec2(i,j))*texelFetch(velDiff, ivec2(frac_x)+ivec2(i,j),0).x;
	//vec2 frac_y = pos*vec2(size)-vec2(1., .5);
	vec2 frac_y = pos*vec2(size)-vec2(.5, .0);
	for(int i = 0; i<2; ++i)
		for(int j = 0; j<2; ++j)
			res[1] += bilin_kernel_grad(frac_y-floor(frac_y)-vec2(i,j))*texelFetch(velDiff, ivec2(frac_y)+ivec2(i,j),0).y;

	return res;
}
float length1(vec2 x) {
	return abs(x.x)+abs(x.y);
}

float solidDist(vec2 uv, float eval_t) {
	//float walls = 1e8;//min(uv.x-.01, 1.-uv.x-.01);
	float angle = .0;//1.6;//-.1*eval_t;
	vec2 cs = vec2(cos(angle), sin(angle));
	//uv.y+=.25;//uv.x += cos(eval_t*.25)*.55;
	uv = mat2(cs.x,cs.y,-cs.y,cs.x)*(uv-vec2(.5));
	//return max(.4-length(uv), length(uv)-.5);
	
	//return min(.45-abs(uv.x), uv.y+.45);//, max(.2-length(uv), length(uv)-.3)));
	return min(.45-abs(uv.x), min(length(uv-vec2(-.2, -.45))-.05, uv.y+.45));//, max(.2-length(uv), length(uv)-.3)));
	//vec2 d = abs(uv)-vec2(.02, .2);
	//return length(max(d,.0))+min(max(d.x,d.y),0.);
}
vec2 solidGrad(vec2 uv) {
	const float eps = 1e-4;
	vec2 res = vec2(solidDist(uv+vec2(eps,.0),t),solidDist(uv+vec2(.0, eps),t))
		-vec2(solidDist(uv+vec2(-eps,.0),t),solidDist(uv+vec2(.0, -eps),t));
	float l = length(res);
	if(l>.0) res /= l;
	return res;
}

vec2 move2(vec2 pos, vec2 dist) {
	float l = length(dist);
	if(l==.0) return pos;
	ivec2 res = textureSize(fluidVol,0);
	vec2 gpos = pos * vec2(res);
	ivec2 ipos = ivec2(gpos), target = ivec2((pos+dist)*vec2(res));
	if(ipos==target) return pos + dist;
	ivec2 step; vec2 curr, delta = 1./(abs(dist*vec2(res)));
	if(dist.x>.0) {
		step.x = 1;
		curr.x = ceil(gpos.x)-gpos.x;
	} else {
		step.x = -1;
		curr.x = gpos.x-floor(gpos).x;
	}
	if(dist.y>.0) {
		step.y = 1;
		curr.y = ceil(gpos.y)-gpos.y;
	} else {
		step.y = -1;
		curr.y = gpos.y-floor(gpos.y);
	}
	curr *= delta;
	float t = .0;

	while(ipos!=target) {
		if(curr.x<curr.y || isnan(curr.y)) {
			curr.x += delta.x;
			t += delta.x;
			ipos.x += step.x;
		} else {
			curr.y += delta.y;
			t += delta.y;
			ipos.y += step.y;
		}
		if(texelFetch(fluidVol, ipos,0).z==.0) break;
	}
	vec2 v = abs(dist*vec2(res));
	return pos + t/(v.x+v.y)*dist/l;
}

vec2 move(vec2 pos, vec2 dist) {
	pos += dist;
	ivec2 res = textureSize(fluidVol, 0);
	//while(pos.x>=.0&&pos.y>=.0&&pos.x<1.&&pos.y<1.&&texelFetch(fluidVol, ivec2(pos*res),0).z==.0) pos += .5*solidGrad(pos-vec2(1.)/vec2(res))/vec2(res);
	return pos;
}

void main() {
	int index = int(gl_GlobalInvocationID.x);
	if(mode == 0) { // init
		int w = int(sqrt(particle_pos.length()));
		vec2 uv = vec2(index/w, index%w)/float(w);
		particle_pos[index] = vec2(.4,.05)+uv*vec2(.2,.3+.01*sin(float(index*192492)+.1));//+vec2(.0, .5);
		//particle_vel[index] = 5.*vec2(uv.y-.5, uv.x-.5);
		particle_vel[index] = vec2(.0);//.5*vec2(-(uv.x-.5), -(uv.y-.5))+.5*vec2(uv.y-.5, .5-uv.x);
		affine[index] = vec4(.0);
	} else if(mode == 1) { // grid to particle + advect
		vec2 pos = particle_pos[index];

		vec2 new_vel = eval_vel(pos);
		vec2 veld = eval_vel_diff(pos);
		mat2 m = eval_affine(pos);
		mat2 md = eval_affine_diff(pos);

		vec2 k1 = eval_vel(pos);
		vec2 k2 = eval_vel(pos+.5*dt*k1);
		vec2 k3 = eval_vel(pos+.75*dt*k2);

		pos += (2.*k1+3.*k2+4.*k3)*dt/9.;

		particle_pos[index] = pos;
		//particle_vel[index] = new_vel;
		//affine[index] = vec4(m[0],m[1]);
		particle_vel[index] = veld;
		affine[index] = vec4(md[0],md[1]);
	}
}
