
#version 450

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba32f) uniform image2D fluidVolume, densityImage;
layout(rgba16f) uniform image2D result;
layout(rg32f) uniform image2D velocity, velDifference, boundaryVelocity;
layout(r32f) uniform image2D pressure, divergenceImage;
uniform sampler2D meme, oldVelocity, oldResult, oldPressure, divergence, fluidVol;

uniform int mode;

uniform float dt;

vec4 interpVel(vec2 uv) {
	ivec2 size = textureSize(oldVelocity, 0);

	ivec2 coordx = ivec2(uv*vec2(size)-vec2(.5));
	vec2 px = vec2(uv*vec2(size)-vec2(coordx))-vec2(.5);

	//printf("uv: %^2g\ncoordx: %^2d\npx: %^2g\ngrid coord: %^2g\n\n", uv, coordx, px, vec2(uv*vec2(size)-vec2(coordx)));

	float x =
	(1.-px.x)*(1.-px.y)*texelFetch(oldVelocity, coordx,0).x+
	px.x*(1.-px.y)*texelFetch(oldVelocity, coordx+ivec2(1,0),0).x+
	(1.-px.x)*px.y*texelFetch(oldVelocity, coordx+ivec2(0,1),0).x+
	px.x*px.y*texelFetch(oldVelocity, coordx+ivec2(1,1),0).x;

	if(coordx.x<0 || coordx.y<0 || coordx.x>=size.x-1 || coordx.y>=size.y-1)
		x = .0;//texelFetch(oldVelocity, max(ivec2(0), min(size-ivec2(1), coordx.xy)), 0).x;

	ivec2 coordy = ivec2(uv*vec2(size)-vec2(.5));
	vec2 py = vec2(uv*vec2(size)-vec2(coordy))-vec2(.5);
	
	if(coordy.y>=size.y && coordy.x>=246 && coordy.x<266)
		return vec4(.0, -1., .0, .0);
	float y = 
	(1.-py.x)*(1.-py.y)*texelFetch(oldVelocity, coordy,0).y+
	py.x*(1.-py.y)*texelFetch(oldVelocity, coordy+ivec2(1,0),0).y+
	(1.-py.x)*py.y*texelFetch(oldVelocity, coordy+ivec2(0,1),0).y+
	py.x*py.y*texelFetch(oldVelocity, coordy+ivec2(1,1),0).y;

	if(coordy.x<0 || coordy.y<0 || coordy.x>=size.x-1 || coordy.y>=size.y-1)
		y = .0;//texelFetch(oldVelocity, max(ivec2(0), min(size-ivec2(1), coordy.xy)), 0).y;

	return vec4(x, y, .0, .0);
}

vec4 cubic(vec2 uv) {
	ivec2 size = textureSize(oldResult, 0);
	ivec2 coord = ivec2(uv*vec2(size)-vec2(.5));
	vec2 p = vec2(uv*vec2(size)-vec2(coord))-vec2(.5);
	vec4 xw = vec4(.0, 1., .0, .0) + p.x*(vec4(-.5,.0,.5,.0)+p.x*(vec4(1.,-5./2.,2.,-.5)+p.x*vec4(-.5,1.5,-1.5,.5)));
	vec4 yw = vec4(.0, 1., .0, .0) + p.y*(vec4(-.5,.0,.5,.0)+p.y*(vec4(1.,-5./2.,2.,-.5)+p.y*vec4(-.5,1.5,-1.5,.5)));
	vec4 result = vec4(.0);
	vec4 mins = vec4(1e16);
	vec4 maxs = vec4(-1e16);
	for(int i = 0; i<4; ++i)
		for(int j = 0; j<4; ++j) {
			ivec2 pcoord = coord+ivec2(i-1,j-1);
			vec4 val = texelFetch(oldResult, pcoord,0);
			if(pcoord.x<1||pcoord.y<1||pcoord.x>=size.x-1||pcoord.y>=size.y-1) val = vec4(.0);
			if(pcoord.y>=size.y-1 && pcoord.x>=246 && pcoord.x<266)
				val = vec4(1.);
			mins = min(val, mins);
			maxs = max(val, maxs);
			result += xw[i]*yw[j]*val;
		}
	return max(min(result, maxs), mins);
}

vec4 cubicVel(vec2 uv) {
	ivec2 size = textureSize(oldResult, 0);
	ivec2 coord = ivec2(uv*vec2(size)-vec2(.5));
	vec2 p = vec2(uv*vec2(size)-vec2(coord))-vec2(.5);

	vec4 xw = vec4(.0, 1., .0, .0) + p.x*(vec4(-.5,.0,.5,.0)+p.x*(vec4(1.,-5./2.,2.,-.5)+p.x*vec4(-.5,1.5,-1.5,.5)));
	vec4 yw = vec4(.0, 1., .0, .0) + p.y*(vec4(-.5,.0,.5,.0)+p.y*(vec4(1.,-5./2.,2.,-.5)+p.y*vec4(-.5,1.5,-1.5,.5)));
	vec4 result = vec4(.0), r2 = vec4(.0);
	vec4 mins = vec4(1e16);
	vec4 maxs = vec4(-1e16);
	for(int i = 0; i<4; ++i)
		for(int j = 0; j<4; ++j) {
			ivec2 pcoord = coord+ivec2(i-1,j-1);
			vec4 val = texelFetch(oldVelocity, pcoord, 0);
			if(pcoord.x<1||pcoord.y<1||pcoord.x>=size.x-1||pcoord.y>=size.y-1) val = vec4(.0);
			if(pcoord.y>=size.y-1 && pcoord.x>=246 && pcoord.x<266)
				val = vec4(.0, -1., .0, .0);
			mins = min(val, mins);
			maxs = max(val, maxs);
			result += xw[i]*yw[j]*val;
		}
	return max(min(result, maxs), mins);
}

const float density = 1.;
uniform float phase;

uniform float t;

float solidDist(vec2 uv, float eval_t) {
	//float walls = 1e8;//min(uv.x-.01, 1.-uv.x-.01);
	float angle = .9*eval_t-1.;
	vec2 cs = vec2(cos(angle), sin(angle));
	uv.y += .25;//cos(eval_t*2.25)*.15;
	uv = mat2(cs.x,cs.y,-cs.y,cs.x)*(uv-vec2(.5));
	
	//return max(uv.y, max(.2-length(uv), length(uv)-.3));
	
	vec2 d = abs(uv)-vec2(.02, .2);
	return length(max(d,.0))+min(max(d.x,d.y),0.);
}

float volume(vec2 uv) {
	float dx = .5/float(textureSize(oldResult,0).x);
	float diag = sqrt(2.*dx*dx);
	float x = smoothstep(-diag, diag, solidDist(uv, t));
	if(x>.0) x = max(x,.01);
	return x;
}

vec2 solidGrad(vec2 uv) {
	const float eps = 1e-4;
	return normalize(
		vec2(solidDist(uv+vec2(eps,.0),t),solidDist(uv+vec2(.0, eps),t))
		-vec2(solidDist(uv+vec2(-eps,.0),t),solidDist(uv+vec2(.0, -eps),t))
		);
}

vec2 solidVel(vec2 uv) {
	const float eps = 1e-4;
	vec2 g = solidGrad(uv);
	vec2 v = -g * (solidDist(uv,t+eps)-solidDist(uv,t-eps))/(2.*eps);
	return v;
}

void main() {
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	if(coord==ivec2(200))
		enablePrintf();
	ivec2 size = ivec2(gl_WorkGroupSize.xy*gl_NumWorkGroups.xy);
	const float dx = 1./float(size.x);
	vec2 uv = (vec2(gl_GlobalInvocationID.xy)+vec2(.5))/vec2(size);
	if(mode == 0) { // init
		imageStore(result, coord, vec4(.0));//texture(meme, vec2(uv.x, 1.-uv.y)));
		uv -= vec2(.5);
		imageStore(velocity, coord, vec4(.0, .0,.0,.0));
		imageStore(pressure, coord, vec4(.0));
		imageStore(densityImage, coord, vec4(density));
	} else if(mode == 1) { // advection
		vec2 v = texelFetch(oldVelocity, coord, 0).xy;
		if(coord.x+1<size.x)
			v.x = .5*(v.x+texelFetch(oldVelocity, coord+ivec2(1,0), 0).x);
		if(coord.y+1<size.y)
			v.y = .5*(v.y+texelFetch(oldVelocity, coord+ivec2(0,1), 0).y);

		if(false) { // linear interpolation + euler time integration
			vec4 res = texture(oldResult, uv-dt*v);
			ivec2 ncoord = ivec2(uv*size);
			if(ncoord.y>=size.y-2 && ncoord.x>=246 && ncoord.x<266)
				res = vec4(1.);
			imageStore(result, coord, res); // linear crap
			imageStore(velocity, coord, texture(oldVelocity, uv-dt*v));
		}
		if(false) { // cubic + euler
			imageStore(result, coord, cubic(uv-dt*v));
			imageStore(velocity, coord, cubicVel(uv-dt*v));
		}
		if(false) { // linear + RK2
			vec2 final_uv = uv-dt*interpVel(uv-.5*dt*v).xy;
			imageStore(result, coord, cubic(final_uv));
			imageStore(velocity, coord, interpVel(final_uv));
		}
		if(true) { // cubic + RK2
			vec2 final_uv = uv-dt*cubicVel(uv-.5*dt*v).xy;
			imageStore(result, coord, cubic(final_uv));
			imageStore(velocity, coord, cubicVel(final_uv));
		}
	} else if(mode==2) { // apply forces, set up boundary conditions
		vec2 uv_x = uv-vec2(.5*dx,.0);
		vec2 uv_y = uv-vec2(.0,.5*dx);

		vec3 fluidVol = vec3(volume(uv_x), volume(uv_y), volume(uv));
		imageStore(fluidVolume, coord, vec4(fluidVol, .0));

		float vel_x = solidVel(uv_x).x;
		float vel_y = solidVel(uv_y).y;
		imageStore(boundaryVelocity, coord, vec4(vel_x, vel_y, .0, .0));
		
		vec4 vel = texelFetch(oldVelocity, coord, 0);
		if(coord.y>=size.y-1 && coord.x>=246 && coord.x<266)
			vel.y = -1.;
		//if(coord.y==0)
		//	vel.y = -8.91*density;
		//if(coord.y<size.y-1)
		//	vel.y -= 8.91*dt;
		vel.x = mix(vel_x, vel.x, fluidVol.x);
		vel.y = mix(vel_y, vel.y, fluidVol.y);
		imageStore(velocity, coord, vel);
		imageStore(result, coord, mix(vec4(.0), texelFetch(oldResult, coord, 0), fluidVol.z));
		//imageStore(result, coord, texelFetch(oldResult, coord, 0));

	} else if(mode==3) { // div preprocess
		imageStore(result, coord, texelFetch(oldResult, coord, 0));
		float div = .0;
		vec2 vol_pos = vec2(
			imageLoad(fluidVolume, coord+ivec2(1,0)).x,
			imageLoad(fluidVolume, coord+ivec2(0,1)).y
		);
		vec3 vol = imageLoad(fluidVolume,coord).xyz;
		if(vol.z==.0)
			imageStore(pressure, coord, vec4(.0));
		vec2 diff_pos = vec2(
			texelFetch(oldVelocity, coord+ivec2(1,0), 0).x*vol_pos.x,
			texelFetch(oldVelocity, coord+ivec2(0,1), 0).y*vol_pos.y
		);
		vec2 diff_neg = texelFetch(oldVelocity, coord, 0).xy*vol.xy;
		vec2 diff = diff_pos-diff_neg;
		if((coord.y==size.y-2&&(coord.x<246 || coord.x>=266))) diff.y = .0;
		if(coord.y==0) diff.y = .0;
		if(coord.x==0||coord.x==size.x-2) diff.x = .0;
		const float scale = dx*dx*density/(dx*dt); // density should be in jacobi

		vec2 vel_pos = vec2(
			imageLoad(boundaryVelocity, coord+ivec2(1,0)).x,
			imageLoad(boundaryVelocity, coord+ivec2(0,1)).y
		);
		vec2 vel_neg = imageLoad(boundaryVelocity, coord).xy;

		vec2 vol_correct = vel_pos*(vol_pos-vol.zz)-vel_neg*(vol.xy-vol.zz);
		div = scale * ((vol_correct.x+vol_correct.y)-(diff.x+diff.y));
		imageStore(divergenceImage, coord, vec4(div));
	} else if(mode==4) { // pressure iteration
		vec3 neg = texelFetch(fluidVol,coord,0).xyz;
		if(coord.x<size.x-1 && coord.y<size.y-1 && neg.z>.0) {
			vec2 p = vec2(.0);

			p += neg.x*vec2(coord.x==0?.0:texelFetch(oldPressure, coord-ivec2(1,0), 0).x,1.);
			p += neg.y*vec2(coord.y==0?.0:texelFetch(oldPressure, coord-ivec2(0,1), 0).x,1.);
			p += texelFetch(fluidVol, coord+ivec2(1,0), 0).x*vec2(coord.x==size.x-2?.0:texelFetch(oldPressure, coord+ivec2(1,0), 0).x,1.);
			p += texelFetch(fluidVol, coord+ivec2(0,1), 0).y*vec2(coord.y==size.y-2?.0:texelFetch(oldPressure, coord+ivec2(0,1), 0).x,1.);

			p.x += texelFetch(divergence, coord, 0).x;
			float old = texelFetch(oldPressure, coord, 0).x;
			float new = p.x/p.y;
			new = mix(old, new, .7);
			if(neg.z<1.)
				new = max(.0, new);
			imageStore(pressure, coord, vec4(new));
		}

	} else if(mode==5) { // apply pressures
		const float scale = dt/(density*dx);
		vec2 baseVel = texelFetch(oldVelocity, coord, 0).xy;
		vec2 off_press = vec2(.0);
		if(coord.x>0) off_press.x = texelFetch(oldPressure, coord-ivec2(1,0), 0).x;
		if(coord.y>0) off_press.y = texelFetch(oldPressure, coord-ivec2(0,1), 0).x;
		
		vec2 vel = baseVel - scale * (texelFetch(oldPressure, coord, 0).xx-off_press);
		vec2 neg = texelFetch(fluidVol,coord,0).xy;
		/*vec2 vel_neg = imageLoad(boundaryVelocity, coord).xy;
		vel.x = mix(vel.x, vel_neg.x, 1.-neg.x);
		vel.y = mix(vel.y, vel_neg.y, 1.-neg.y);
		baseVel.x = mix(baseVel.x, vel_neg.x, 1.-neg.x);
		baseVel.y = mix(baseVel.y, vel_neg.y, 1.-neg.y);*/
		imageStore(velDifference, coord, vec4(vel+.98*(vel-baseVel), .0, .0));
		imageStore(velocity, coord, vec4(vel, .0, .0));
	}
}
