
#version 450

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba32f) uniform image2D fluidVolume, densityImage, fluidMass;
layout(rg32f) uniform image2D velocity, velDifference, boundaryVelocity;
layout(r32f) uniform image2D pressure, divergenceImage;
uniform sampler2D oldVelocity, oldDensity, oldPressure, divergence, fluidVol, fluidRelVol;

uniform int mode;

uniform float dt;

uniform float phase;

uniform float t;


float length1(vec2 x) {
	return abs(x.x)+abs(x.y);
}


float solidDist(vec2 uv, float eval_t) {
	//float walls = 1e8;//min(uv.x-.01, 1.-uv.x-.01);
	float angle = .0;//-.1*eval_t;
	vec2 cs = vec2(cos(angle), sin(angle));
	uv = mat2(cs.x,cs.y,-cs.y,cs.x)*(uv-vec2(.5));
	
	//return max(.4-length(uv), length(uv)-.5);
	//return min(.45-abs(uv.x), uv.y+.45);//, max(.2-length(uv), length(uv)-.3)));
	return min(.45-abs(uv.x), min(length(uv-vec2(-.2, -.45))-.05, uv.y+.45));//, max(.2-length(uv), length(uv)-.3)));
	
	//vec2 d = abs(uv)-vec2(.02, .2);
	//return length(max(d,.0))+min(max(d.x,d.y),0.);
}

float volume(vec2 uv) {
	float dx = .5/float(textureSize(oldDensity,0).x);
	float diag = sqrt(2.*dx*dx);
	float x = smoothstep(-diag, diag, solidDist(uv, t));
	if(x>.0) x = max(x,.1);
	return x;
}

vec2 solidGrad(vec2 uv) {
	const float eps = 1e-4;
	vec2 res = vec2(solidDist(uv+vec2(eps,.0),t),solidDist(uv+vec2(.0, eps),t))
		-vec2(solidDist(uv+vec2(-eps,.0),t),solidDist(uv+vec2(.0, -eps),t));
	float l = length(res);
	if(l>.0) res /= l;
	return res;
}

vec2 solidVel(vec2 uv) {
return vec2(.0);
	const float eps = 1e-4;
	vec2 g = solidGrad(uv);
	vec2 v = -g * (solidDist(uv,t+eps)-solidDist(uv,t-eps))/(2.*eps);
	return v;
}

const float density = .0001;
const float air_density = .0;

void main() {
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	if(coord==ivec2(200))
		enablePrintf();
	ivec2 size = ivec2(gl_WorkGroupSize.xy*gl_NumWorkGroups.xy);
	const float dx = 1./float(size.x);
	vec2 uv = (vec2(gl_GlobalInvocationID.xy)+vec2(.5))/vec2(size);
	if(mode == 0) { // init
		uv -= vec2(.5);
		imageStore(velocity, coord, vec4(.0, .0,.0,.0));
		imageStore(pressure, coord, vec4(.0));
		imageStore(densityImage, coord, vec4(.0));
	} else if(mode == 1) { // reset
		imageStore(velocity, coord, vec4(.0, .0,.0,.0));
		imageStore(densityImage, coord, vec4(air_density));
	} else if(mode==2) { // apply forces, set up boundary conditions
		vec2 uv_x = uv-vec2(.5*dx,.0);
		vec2 uv_y = uv-vec2(.0,.5*dx);

		vec3 fluidVol = vec3(volume(uv_x), volume(uv_y), volume(uv));
		//fluidVol = max(fluidVol, 1.-clamp((texelFetch(oldDensity, coord, 0).xyz-vec3(air_density))*100., vec3(.0), vec3(1.)));
		imageStore(fluidVolume, coord, vec4(fluidVol, .0));

		float vel_x = solidVel(uv_x).x;
		float vel_y = solidVel(uv_y).y;
		imageStore(boundaryVelocity, coord, vec4(vel_x, vel_y, .0, .0));
		
		vec4 vel = texelFetch(oldVelocity, coord, 0);
		vec3 dens = texelFetch(oldDensity, coord, 0).xyz;
		//if(fluidVol.z==.0) vel.xy = vec2(.0);
		//else
		{
			for(int i = 0; i<2; ++i)
				if(dens[i] != .0)
					vel[i] /= dens[i];
				else
					vel[i] = .0;
			//if(dens.x != .0 || dens.y != .0)
		}

		imageStore(velocity, coord, vel);

	} else if(mode==6) { // velocity flood fill
		vec3 dens = texelFetch(oldDensity, coord, 0).xyz;
		vec2 closest_vel = vec2(.0);
		vec2 closest = vec2(100.);
		vec2 visc = vec2(.0);
		float viscw = .0;
		for(int i = 0; i<5; ++i) {
			for(int j = 0; j<5; ++j) {
				ivec2 pcoord = coord+ivec2(i-2, j-2);
				if(pcoord.x>=0&&pcoord.y>=0&&pcoord.x<size.x&&pcoord.y<size.y) {
					float d = length(vec2(i-2, j-2));
					vec2 dens_p = texelFetch(oldDensity, pcoord,0).xy;
					vec2 possvel = texelFetch(oldVelocity, pcoord, 0).xy;
					float w = exp(-d*.5);
					visc += w*possvel;
					viscw += w;
					if(dens_p.x>.0 && d<closest.x) {
						closest.x = d;
						closest_vel.x = possvel.x;
					}
					if(dens_p.y>.0 && d<closest.y) {
						closest.y = d;
						closest_vel.y = possvel.y;
					}
				}
			}
		}
		closest_vel.y -= .25*dt;
		closest_vel -= dt*(closest_vel-visc/viscw);
		imageStore(velocity, coord, vec4(closest_vel,.0,.0));
	} else if(mode==3) { // div preprocess
		vec2 vol_pos = vec2(
			imageLoad(fluidVolume, coord+ivec2(1,0)).z,
			imageLoad(fluidVolume, coord+ivec2(0,1)).z
		);
		vec3 vol = vec3(
			imageLoad(fluidVolume,coord-ivec2(1,0)).z,
			imageLoad(fluidVolume,coord-ivec2(0,1)).z,
			imageLoad(fluidVolume,coord).z
		);
		vec2 dens_pos = vec2(
			texelFetch(oldDensity,coord+ivec2(1,0),0).z,
			texelFetch(oldDensity,coord+ivec2(0,1),0).z
		);
		vec3 dens_neg = vec3(
			texelFetch(oldDensity,coord-ivec2(1,0),0).z,
			texelFetch(oldDensity,coord-ivec2(0,1),0).z,
			texelFetch(oldDensity,coord,0).z
		);
		//if(vol.z == .0 || dens_neg.z==.0)
		//	imageStore(pressure, coord, vec4(.0));
		vec2 diff_pos = vec2(
			texelFetch(oldVelocity, coord+ivec2(1,0), 0).x,//*vol_pos.x,
			texelFetch(oldVelocity, coord+ivec2(0,1), 0).y//*vol_pos.y
		);
		vec2 diff_neg = texelFetch(oldVelocity, coord, 0).xy;//*vol.xy;
		if(vol_pos.x == .0) diff_pos.x = .0;
		if(vol_pos.y == .0) diff_pos.y = .0;
		if(vol.x == .0) diff_neg.x = .0;
		if(vol.y == .0) diff_neg.y = .0;
		vec2 diff = diff_pos-diff_neg;

		//if(coord.x==0||coord.x==size.x-2||dens_pos.x == .0||dens_neg.x == .0) diff.x = .0;
		//if(coord.y==0||coord.y==size.y-2||dens_pos.y == .0||dens_neg.y == .0) diff.y = .0;

		if(coord.x==0||coord.x==size.x-2) diff.x = .0;
		if(coord.y==0||coord.y==size.y-2) diff.y = .0;

		vec2 vel_pos = vec2(
			imageLoad(boundaryVelocity, coord+ivec2(1,0)).x,
			imageLoad(boundaryVelocity, coord+ivec2(0,1)).y
		);
		vec2 vel_neg = imageLoad(boundaryVelocity, coord).xy;

		vec2 vol_correct = vel_pos*(vol_pos-vol.zz)-vel_neg*(vol.xy-vol.zz);
		//float div = dens.z*dx/(dt) * ((vol_correct.x+vol_correct.y)-(diff.x+diff.y));
		float div = density*(dx/dt) * (-(diff.x+diff.y));
		//if(vol.z>.0&&dens_neg.z>.0)
		float corr = .0;
		if(coord.x<size.x-1 && texelFetch(oldDensity, coord+ivec2(1,0),0).z>.0)
			corr += texelFetch(oldDensity, coord+ivec2(1,0),0).x-1.;
		if(coord.y<size.y-1 && texelFetch(oldDensity, coord+ivec2(0,1),0).z>.0)
			corr += texelFetch(oldDensity, coord+ivec2(0,1),0).y-1.;
		if(coord.x>0 && texelFetch(oldDensity, coord-ivec2(1,0),0).z>.0)
			corr += texelFetch(oldDensity, coord-ivec2(1,0),0).x-1.;
		if(coord.y>0 && texelFetch(oldDensity, coord-ivec2(0,1),0).z>.0)
			corr += texelFetch(oldDensity, coord-ivec2(0,1),0).y-1.;
		//div += .0000001*density*corr;
		imageStore(divergenceImage, coord, vec4(div));
		imageStore(fluidMass, coord, vec4(vol, .0));

	} else if(mode==4) { // pressure iteration
		vec3 vol_neg = vec3(
			texelFetch(fluidRelVol,coord-ivec2(1,0),0).z,
			texelFetch(fluidRelVol,coord-ivec2(0,1),0).z,
			texelFetch(fluidRelVol,coord,0).z
		);
		vec3 dens_neg = vec3(
			texelFetch(oldDensity,coord-ivec2(1,0),0).z,
			texelFetch(oldDensity,coord-ivec2(0,1),0).z,
			texelFetch(oldDensity,coord,0).z
		);
		if(coord.x<size.x-1 && coord.y<size.y-1 && vol_neg.z>.0 && dens_neg.z>.0) {
			vec2 vol_pos = vec2(
				texelFetch(fluidRelVol,coord+ivec2(1,0),0).z,
				texelFetch(fluidRelVol,coord+ivec2(0,1),0).z
			);
			vec2 dens_pos = vec2(
				texelFetch(oldDensity,coord+ivec2(1,0),0).z,
				texelFetch(oldDensity,coord+ivec2(0,1),0).z
			);
			vec2 p = vec2(.0);
			if(vol_neg.x>.0) {
				p.y += 1.;
				if(coord.x>0) {
					if(dens_neg.x>.0){
						p += texelFetch(oldPressure, coord-ivec2(1,0), 0).x;
					}
				}
			}
			if(vol_neg.y>.0) {
				p.y += 1.;
				if(coord.y>0) {
					if(dens_neg.y>.0){
						p += texelFetch(oldPressure, coord-ivec2(0,1), 0).x;
					}
				}
			}
			if(vol_pos.x>.0) {
				p.y += 1.;
				if(coord.x<size.x-2) {
					if(dens_pos.x>.0){
						p += texelFetch(oldPressure, coord+ivec2(1,0), 0).x;
					}
				}
			}
			if(vol_pos.y>.0) {
				p.y += 1.;
				if(coord.y<size.y-2) {
					if(dens_pos.y>.0) {
						p += texelFetch(oldPressure, coord+ivec2(0,1), 0).x;
					}
				}
			}
			p += vec2(texelFetch(divergence, coord, 0).x, .0);
			//if(p.y==.0) p = vec2(.0,1.);
			float new = mix(texelFetch(oldPressure, coord, 0).x, p.x/p.y, .6);
			//if(vol_neg.z==.0)
				new = max(.0, new);
			imageStore(pressure, coord, vec4(new));
		}

	} else if(mode==5) { // apply pressures
		vec3 dens = texelFetch(oldDensity,coord,0).xyz;
		vec2 scale = vec2(dt/(density*dx));
		vec2 baseVel = texelFetch(oldVelocity, coord, 0).xy;
		vec2 off_press = vec2(.0);

		vec3 vol_neg = vec3(
			texelFetch(fluidRelVol,coord-ivec2(1,0),0).z,
			texelFetch(fluidRelVol,coord-ivec2(0,1),0).z,
			texelFetch(fluidRelVol,coord,0).z
		);
		vec3 dens_neg = vec3(
			texelFetch(oldDensity,coord-ivec2(1,0),0).z,
			texelFetch(oldDensity,coord-ivec2(0,1),0).z,
			texelFetch(oldDensity,coord,0).z
		);

		if(coord.x>0 && vol_neg.x>.0) off_press.x = texelFetch(oldPressure, coord-ivec2(1,0), 0).x;
		if(coord.y>0 && vol_neg.y>.0) off_press.y = texelFetch(oldPressure, coord-ivec2(0,1), 0).x;
		
		vec2 vel = baseVel - scale * (texelFetch(oldPressure, coord, 0).xx-off_press);
		
		
		//if(dens_neg.x>.0)
		//	baseVel.x -= 2.*dt*(dens_neg.x-dens_neg.z);
		//if(dens_neg.y>.0)
		//	baseVel.y -= 2.*dt*(dens_neg.y-dens_neg.z);

		if(vol_neg.z==.0) vel = min(vel, vec2(.0));

		if(vol_neg.x==.0 && coord.x>0) vel.x = max(vel.x, .0);
		if(vol_neg.y==.0 && coord.y>0) vel.y = max(vel.y, .0);
		
		//if(coord.x==.0||coord.x==size.x-1) baseVel = vel;
		//if(coord.y==.0||coord.y==size.y-1) baseVel = vel;

		imageStore(velDifference, coord, vec4(vel+(vel-baseVel), .0, .0));
		imageStore(velocity, coord, vec4(vel, .0, .0));
	}
}
