
#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(rgba32f) uniform image3D fluidVolume, densityImage, shade;
layout(rgba32f) uniform image3D velocity, velDifference, boundaryVelocity;
layout(r32f) uniform image3D pressure, divergenceImage;
uniform sampler3D oldVelocity, oldDensity, oldPressure, divergence, fluidVol, fluidRelVol;

uniform int mode, size;

uniform float dt, t, dx;

uniform int scene, frame;
uniform int strobo;

const vec3 spherecenters[4] = vec3[4](vec3(-.2, -.2, -.2),vec3(.28, .2, -.3),vec3(.0, .28, .0),vec3(.2, -.3, .1));


float solidDist(vec3 uv, float eval_t) {
	uv -= vec3(.5);
	float box = min(min(.45-abs(uv.z), .45-abs(uv.x)), .45-abs(uv.y));
	if(scene==0) {
		return min(length(uv-vec3(-.5, -.5, -.5))-.3, box);
	}
	if(scene==1) {
		return .45-abs(uv.x);
	}
	if(scene==2) {
		return box;
	}
	if(scene==3||scene==6||scene==8) {
		return min(box,.05*(2.*sin(uv.x*2.15-uv.z*4.2)+1.5*sin(uv.x*23.+uv.z*30.)+sin(uv.z*45.-uv.x*41.))+.4-3.*abs(uv.y));
	}
	if(scene==4||scene==7) {
		return min(box,min(min(length(uv.xz-vec2(-.1,-.24))-.1,length(uv.xz-vec2(-.1,.24))-.1),length(uv.xz-vec2(.3, .0))-.1));
	}
	if(scene==5) {
		float s = .0;
		for(int i = 0; i<4; ++i)
			s = max(s, .2-length(uv-spherecenters[i]));
		return s;
	}
}

float volume(vec3 uv) {
	float diag = dx;
	float x = smoothstep(.0, diag, solidDist(uv, t));
	//if(x>.0) x = max(x,.02);
	return x;
}

vec3 solidGrad(vec3 uv) {
	const float eps = 1e-4;
	vec3 res = vec3(solidDist(uv+vec3(eps,.0,.0),t),solidDist(uv+vec3(.0,eps,.0),t),solidDist(uv+vec3(.0,.0,eps),t))
		-vec3(solidDist(uv-vec3(eps,.0,.0),t),solidDist(uv-vec3(.0,eps,.0),t),solidDist(uv-vec3(.0,.0,eps),t));
	float l = length(res);
	if(l>.0) res /= l;
	return res;
}

const float density = .0000001;

void main() {
	ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);

	vec3 uv = (vec3(gl_GlobalInvocationID.xyz)+vec3(.5))*dx;
	if(mode == 0) { // init
		uv -= vec3(.5);
		imageStore(velocity, coord, vec4(.0, .0,.0,.0));
		imageStore(pressure, coord, vec4(.0));
		imageStore(densityImage, coord, vec4(.0));
	} else if(mode == 1) { // reset
		imageStore(velocity, coord, vec4(.0, .0,.0,.0));
		imageStore(densityImage, coord, vec4(.0));
		imageStore(velDifference, coord, vec4(.0));
	} else if(mode==2) { // apply forces, set up boundary conditions
		vec3 uv_x = uv-vec3(.5*dx,.0,.0);
		vec3 uv_y = uv-vec3(.0,.5*dx,.0);
		vec3 uv_z = uv-vec3(.0,.0,.5*dx);

		vec4 fluidVol = vec4(volume(uv_x), volume(uv_y), volume(uv_z), volume(uv));
		imageStore(fluidVolume, coord, fluidVol);

		vec4 vel = texelFetch(oldVelocity, coord, 0);
		vec4 dens = texelFetch(oldDensity, coord, 0);
		for(int i = 0; i<3; ++i)
			if(dens[i] != .0)
				vel[i] /= dens[i];
			else
				vel[i] = .0;

		imageStore(velocity, coord, vel);
		for(int i = 0; i<4; ++i)
		for(int j = 0; j<4; ++j)
		for(int k = 0; k<4; ++k)
		imageStore(shade, coord+ivec3(i,j,k)*64, vec4(.0));
	} else if(mode==6) { // velocity flood fill
		vec4 dens = texelFetch(oldDensity, coord, 0);
		vec3 closest_vel = vec3(.0);
		vec3 closest = vec3(100.);
		vec3 visc = vec3(.0);
		float viscw = .0;
		for(int i = 0; i<3; ++i) {
			for(int j = 0; j<3; ++j) {
				for(int k = 0; k<3; ++k) {
					ivec3 pcoord = coord+ivec3(i-1, j-1, k-1);
					if(pcoord.x>=0&&pcoord.y>=0&&pcoord.z>=0&&pcoord.x<size&&pcoord.y<size&&pcoord.z<size) {
						float d = length(vec3(i-1, j-1, k-1));
						vec3 dens_p = texelFetch(oldDensity, pcoord,0).xyz;
						vec3 possvel = texelFetch(oldVelocity, pcoord, 0).xyz;
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
						if(dens_p.z>.0 && d<closest.z) {
							closest.z = d;
							closest_vel.z = possvel.z;
						}
					}
				}
			}
		}
		if(scene==5)
			closest_vel.xz -= 8.*dt*vec2(cos(float(frame)*.0862), .1*sin(float(frame)*.0862));
		else
			closest_vel.y -= .25*dt;
		//closest_vel -= .05*dt*(closest_vel-visc/viscw);
		imageStore(velocity, coord, vec4(closest_vel,.0));
	} else if(mode==3) { // div preprocess
		vec3 vol_pos = vec3(
			imageLoad(fluidVolume, coord+ivec3(1,0,0)).w,
			imageLoad(fluidVolume, coord+ivec3(0,1,0)).w,
			imageLoad(fluidVolume, coord+ivec3(0,0,1)).w
		);
		vec4 vol = vec4(
			imageLoad(fluidVolume,coord-ivec3(1,0,0)).w,
			imageLoad(fluidVolume,coord-ivec3(0,1,0)).w,
			imageLoad(fluidVolume,coord-ivec3(0,0,1)).w,
			imageLoad(fluidVolume,coord).w
		);
		vec3 dens_pos = vec3(
			texelFetch(oldDensity, coord+ivec3(1,0,0),0).w,
			texelFetch(oldDensity, coord+ivec3(0,1,0),0).w,
			texelFetch(oldDensity, coord+ivec3(0,0,1),0).w
		);
		vec4 dens_neg = vec4(
			texelFetch(oldDensity,coord-ivec3(1,0,0),0).w,
			texelFetch(oldDensity,coord-ivec3(0,1,0),0).w,
			texelFetch(oldDensity,coord-ivec3(0,0,1),0).w,
			texelFetch(oldDensity,coord,0).w
		);
		//if(vol.w == .0 || dens_neg.w==.0)
		if(frame==0)
			imageStore(pressure, coord, vec4(.0));
		vec3 diff_pos = vec3(
			texelFetch(oldVelocity, coord+ivec3(1,0,0), 0).x*imageLoad(fluidVolume, coord+ivec3(1,0,0)).x,
			texelFetch(oldVelocity, coord+ivec3(0,1,0), 0).y*imageLoad(fluidVolume, coord+ivec3(0,1,0)).y,
			texelFetch(oldVelocity, coord+ivec3(0,0,1), 0).z*imageLoad(fluidVolume, coord+ivec3(0,0,1)).z
		);
		vec3 diff_neg = texelFetch(oldVelocity, coord, 0).xyz*imageLoad(fluidVolume,coord).xyz;

		if(vol_pos.x == .0) diff_pos.x = .0;
		if(vol_pos.y == .0) diff_pos.y = .0;
		if(vol_pos.z == .0) diff_pos.z = .0;

		if(vol.x == .0) diff_neg.x = .0;
		if(vol.y == .0) diff_neg.y = .0;
		if(vol.z == .0) diff_neg.z = .0;

		vec3 diff = diff_pos-diff_neg;

		if(coord.x==0||coord.x==size-2) diff.x = .0;
		if(coord.y==0||coord.y==size-2) diff.y = .0;
		if(coord.z==0||coord.z==size-2) diff.z = .0;

		float div = density*(dx/dt) * (-(diff.x+diff.y+diff.z));

		if(dens_neg.w>.0) {
			float corr = .0;
			float target_density = 4.;
			if(coord.x<size-1 && texelFetch(oldDensity, coord+ivec3(1,0,0),0).w>.0)
				corr += texelFetch(oldDensity, coord+ivec3(1,0,0),0).x-target_density;
			if(coord.y<size-1 && texelFetch(oldDensity, coord+ivec3(0,1,0),0).w>.0)
				corr += texelFetch(oldDensity, coord+ivec3(0,1,0),0).y-target_density;
			if(coord.z<size-1 && texelFetch(oldDensity, coord+ivec3(0,0,1),0).w>.0)
				corr += texelFetch(oldDensity, coord+ivec3(0,0,1),0).z-target_density;
			if(coord.x>0 && texelFetch(oldDensity, coord-ivec3(1,0,0),0).w>.0)
				corr += texelFetch(oldDensity, coord-ivec3(1,0,0),0).x-target_density;
			if(coord.y>0 && texelFetch(oldDensity, coord-ivec3(0,1,0),0).w>.0)
				corr += texelFetch(oldDensity, coord-ivec3(0,1,0),0).y-target_density;
			if(coord.z>0 && texelFetch(oldDensity, coord-ivec3(0,0,1),0).w>.0)
				corr += texelFetch(oldDensity, coord-ivec3(0,0,1),0).z-target_density;
			div += .00005*density*corr;
		}
		imageStore(divergenceImage, coord, vec4(div));

	} else if(mode==4) { // pressure iteration
		vec4 vol_neg = vec4(
			texelFetch(fluidRelVol,coord-ivec3(1,0,0),0).w,
			texelFetch(fluidRelVol,coord-ivec3(0,1,0),0).w,
			texelFetch(fluidRelVol,coord-ivec3(0,0,1),0).w,
			texelFetch(fluidRelVol,coord,0).w
		);
		vec4 dens_neg = vec4(
			texelFetch(oldDensity,coord-ivec3(1,0,0),0).w,
			texelFetch(oldDensity,coord-ivec3(0,1,0),0).w,
			texelFetch(oldDensity,coord-ivec3(0,0,1),0).w,
			texelFetch(oldDensity,coord,0).w
		);
		if(coord.x<size-1 && coord.y<size-1 && coord.z<size-1 && vol_neg.w>.0 && dens_neg.w>.0) {
			vec3 vol_pos = vec3(
				texelFetch(fluidRelVol,coord+ivec3(1,0,0),0).w,
				texelFetch(fluidRelVol,coord+ivec3(0,1,0),0).w,
				texelFetch(fluidRelVol,coord+ivec3(0,0,1),0).w
			);
			vec3 dens_pos = vec3(
				texelFetch(oldDensity,coord+ivec3(1,0,0),0).w,
				texelFetch(oldDensity,coord+ivec3(0,1,0),0).w,
				texelFetch(oldDensity,coord+ivec3(0,0,1),0).w
			);
			vec2 p = vec2(.0);
			p.y += texelFetch(fluidRelVol, coord, 0).x;
			if(coord.x>0) {
				if(dens_neg.x>.0){
					p.x += texelFetch(oldPressure, coord-ivec3(1,0,0), 0).x * texelFetch(fluidRelVol, coord, 0).x;
				}
			}
			p.y += texelFetch(fluidRelVol, coord, 0).y;
			if(coord.y>0) {
				if(dens_neg.y>.0){
					p.x += texelFetch(oldPressure, coord-ivec3(0,1,0), 0).x * texelFetch(fluidRelVol, coord, 0).y;
				}
			}
			p.y += texelFetch(fluidRelVol, coord, 0).z;
			if(coord.z>0) {
				if(dens_neg.z>.0){
					p.x += texelFetch(oldPressure, coord-ivec3(0,0,1), 0).x * texelFetch(fluidRelVol, coord, 0).z;
				}
			}
			p.y += texelFetch(fluidRelVol, coord+ivec3(1,0,0), 0).x;
			if(coord.x<size-2) {
				if(dens_pos.x>.0){
					p.x += texelFetch(oldPressure, coord+ivec3(1,0,0), 0).x*texelFetch(fluidRelVol, coord+ivec3(1,0,0), 0).x;
				}
			}
			p.y += texelFetch(fluidRelVol, coord+ivec3(0,1,0), 0).y;
			if(coord.y<size-2) {
				if(dens_pos.y>.0) {
					p.x += texelFetch(oldPressure, coord+ivec3(0,1,0), 0).x*texelFetch(fluidRelVol, coord+ivec3(0,1,0), 0).y;
				}
			}
			p.y += texelFetch(fluidRelVol, coord+ivec3(0,0,1), 0).z;
			if(coord.z<size-2) {
				if(dens_pos.z>.0) {
					p.x += texelFetch(oldPressure, coord+ivec3(0,0,1), 0).x*texelFetch(fluidRelVol, coord+ivec3(0,0,1), 0).z;
				}
			}
			p += vec2(texelFetch(divergence, coord, 0).x, .0);
			if(p.y==.0) p = vec2(.0,1.);
			float new = mix(texelFetch(oldPressure, coord, 0).x, p.x/p.y, .6);
			//if(vol_neg.w<1.0)
				new = max(.0, new);
			imageStore(pressure, coord, vec4(new));
		}

	} else if(mode==5) { // apply pressures
		vec4 dens = texelFetch(oldDensity,coord,0);
		vec3 scale = vec3(dt/(density*dx));
		vec3 baseVel = texelFetch(oldVelocity, coord, 0).xyz;
		vec3 off_press = vec3(.0);

		vec4 vol_neg = vec4(
			texelFetch(fluidRelVol,coord-ivec3(1,0,0),0).w,
			texelFetch(fluidRelVol,coord-ivec3(0,1,0),0).w,
			texelFetch(fluidRelVol,coord-ivec3(0,0,1),0).w,
			texelFetch(fluidRelVol,coord,0).w
		);
		vec4 dens_neg = vec4(
			texelFetch(oldDensity,coord-ivec3(1,0,0),0).w,
			texelFetch(oldDensity,coord-ivec3(0,1,0),0).w,
			texelFetch(oldDensity,coord-ivec3(0,0,1),0).w,
			texelFetch(oldDensity,coord,0).w
		);

		if(dens_neg.w==.0||dens_neg.x==.0) {
			vec3 y = vec3(dens_neg.x, texelFetch(oldDensity,coord,0).x, dens_neg.w);
			mat2x3 m;
			m[0] = vec3(1.);
			m[1] = vec3(-.5,.0,.5);
			vec2 b = inverse(transpose(m)*m)*transpose(m)*y;
			scale.x *= clamp(-b.y/b.x+.5, .0, 1.);
		}
		if(dens_neg.w==.0||dens_neg.y==.0) {
			vec3 y = vec3(dens_neg.y, texelFetch(oldDensity,coord,0).y, dens_neg.w);
			mat2x3 m;
			m[0] = vec3(1.);
			m[1] = vec3(-.5,.0,.5);
			vec2 b = inverse(transpose(m)*m)*transpose(m)*y;
			scale.y *= clamp(-b.y/b.x+.5, .0, 1.);
		}
		if(dens_neg.w==.0||dens_neg.z==.0) {
			vec3 y = vec3(dens_neg.z, texelFetch(oldDensity,coord,0).z, dens_neg.w);
			mat2x3 m;
			m[0] = vec3(1.);
			m[1] = vec3(-.5,.0,.5);
			vec2 b = inverse(transpose(m)*m)*transpose(m)*y;
			scale.z *= clamp(-b.y/b.x+.5, .0, 1.);
		}

		if(coord.x>0 && vol_neg.x>.0) off_press.x = texelFetch(oldPressure, coord-ivec3(1,0,0), 0).x;
		if(coord.y>0 && vol_neg.y>.0) off_press.y = texelFetch(oldPressure, coord-ivec3(0,1,0), 0).x;
		if(coord.z>0 && vol_neg.z>.0) off_press.z = texelFetch(oldPressure, coord-ivec3(0,0,1), 0).x;
		
		vec3 vel = baseVel - scale * (texelFetch(oldPressure, coord, 0).xxx-off_press);
		
		if(vol_neg.w<1.0) vel = min(vel, vec3(.0));

		if(vol_neg.x<1.0 && coord.x>0) vel.x = max(vel.x, .0);
		if(vol_neg.y<1.0 && coord.y>0) vel.y = max(vel.y, .0);
		if(vol_neg.z<1.0 && coord.z>0) vel.z = max(vel.z, .0);
		
		imageStore(velDifference, coord, vec4(vel+(vel-baseVel), .0));
		imageStore(velocity, coord, vec4(vel, .0));
	} else if (mode==7) {
		ivec2 pos = ivec2(gl_LocalInvocationIndex%256, gl_LocalInvocationIndex/256+2*gl_WorkGroupID.x);
		vec3 col = vec3(1., .9, .8);
		vec3 amb = vec3(.0005, .0014, .002)*2.;
		if(strobo == 1) {
			float stroboflash = 10.0*max(0.,-.5+cos(0.5+t*105./60.*20.*3.141));
			
			col = max(vec3(.05), col * stroboflash);
			amb /= min(vec3(1.),max(vec3(.2),4.*stroboflash));
		}
		vec3 dens = vec3(1.);
		for(int i = 0; i<256; ++i) {
			dens = clamp(amb + dens * clamp(1.-5.*imageLoad(shade, ivec3(pos,255-i).xzy).xxx, .0, 1.), .0, 1.);
			imageStore(shade, ivec3(pos,255-i).xzy, vec4(col*dens,1.));
		}
	}
}
