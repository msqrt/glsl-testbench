
#version 450

layout (local_size_x = 256) in;

uniform uint frame, tick;
uniform ivec2 screenSize;

uniform int rndDimensions;
buffer values{vec4 color[];};
layout(std430) buffer coordinates{vec2 coords[];};
layout(std430) buffer randoms{float rnd[];};

uint rndSeedA, rndSeedB, rndSeedC;
void srand(uint A, uint B, uint C) {rndSeedA = A; rndSeedB = B; rndSeedC = C;}
int rndCounter = 0;
float rand() {
	//if(rndCounter<rndDimensions)
	//	return rnd[gl_GlobalInvocationID.x+(rndCounter++)*color.length()];
	rndSeedA -= rndSeedB; rndSeedA -= rndSeedC; rndSeedA ^= rndSeedC>>13;
	rndSeedB -= rndSeedC; rndSeedB -= rndSeedA; rndSeedB ^= rndSeedA<<8;
	rndSeedC -= rndSeedA; rndSeedC -= rndSeedB; rndSeedC ^= rndSeedB>>13;
	rndSeedA -= rndSeedB; rndSeedA -= rndSeedC; rndSeedA ^= rndSeedC>>12;
	rndSeedB -= rndSeedC; rndSeedB -= rndSeedA; rndSeedB ^= rndSeedA<<16;
	rndSeedC -= rndSeedA; rndSeedC -= rndSeedB; rndSeedC ^= rndSeedB>>5;
	rndSeedA -= rndSeedB; rndSeedA -= rndSeedC; rndSeedA ^= rndSeedC>>3;
	rndSeedB -= rndSeedC; rndSeedB -= rndSeedA; rndSeedB ^= rndSeedA<<10;
	rndSeedC -= rndSeedA; rndSeedC -= rndSeedB; rndSeedC ^= rndSeedB>>15;
	return float(rndSeedC)/pow(2.,32.);
}
float xFit_1931( float wave )
{
	float t1 = (wave-442.0)*((wave<442.0)?0.0624:0.0374);
	float t2 = (wave-599.8)*((wave<599.8)?0.0264:0.0323);
	float t3 = (wave-501.1)*((wave<501.1)?0.0490:0.0382);
	return 0.362*exp(-0.5*t1*t1) + 1.056*exp(-0.5*t2*t2) - 0.065*exp(-0.5*t3*t3);
}
float yFit_1931( float wave )
{
	float t1 = (wave-568.8)*((wave<568.8)?0.0213:0.0247);
	float t2 = (wave-530.9)*((wave<530.9)?0.0613:0.0322);
	return 0.821*exp(-0.5*t1*t1) + 0.286*exp(-0.5*t2*t2);
}
float zFit_1931( float wave )
{
	float t1 = (wave-437.0)*((wave<437.0)?0.0845:0.0278);
	float t2 = (wave-459.0)*((wave<459.0)?0.0385:0.0725);
	return 1.217*exp(-0.5*t1*t1) + 0.681*exp(-0.5*t2*t2);
}

vec3 spectrum(float wave) {
	return transpose(mat3(3.240479,-1.537150,-.498535,-.969256,1.875991,.041556,.055648,-.204043,1.057311))*vec3(xFit_1931(wave), yFit_1931(wave), zFit_1931(wave));
}

const float pi = 3.14159265358979;
mat3 basis(vec3 n) {
    float sg = uintBitsToFloat(floatBitsToUint(1.)|((1<<31)&floatBitsToUint(n.z)));
    float a = -1.0 / (sg + n.z);
    float b = n.x * n.y * a;
    return mat3(vec3(1.0 + sg * n.x * n.x * a, sg * b, -sg * n.x), vec3(b, sg + n.y * n.y * a, -n.y), n);
}

vec3 tonemap(vec3 color) {
	return color * ((1.+length(color)/(1.))/(1.+length(color)));
}

uniform mat4 cameraToWorld;

const float far = 3.e38;

struct raycast {
	vec3 origin; vec3 direction;
};

struct hit {
	float distance;
	vec3 normal;
	int material;
	uint object;
};

hit plane(raycast ray, vec3 normal, vec3 point, int material) {
	hit h; h.material = material; h.normal = normalize(normal);
	float pn = dot(point-ray.origin, h.normal), dn = dot(ray.direction, h.normal);
	h.distance = pn / dn;
	if(h.distance<.0) h.distance = far;
	if(pn>.0) h.distance = -h.distance;
	return h;
}

hit sphere(raycast ray, vec3 position, float radius, int material) {
	hit h; h.material = material;
	vec3 origin = ray.origin - position;
	float a = dot(ray.direction, ray.direction), b = 2.*dot(ray.direction, origin), c = dot(origin,origin)-radius*radius;
	float d = b*b-4.*a*c;
	if(d>=.0) {
		d = sqrt(d);
		a = .5/a;

		h.distance = (-d-b)*a;
		if(h.distance<.0) h.distance = (d-b)*a;
		h.normal = normalize(origin + h.distance*ray.direction);
		if(h.distance<.0) h.distance = far;
	}
	else
		h.distance = far;
	if(c<.0) h.distance = -h.distance;
	return h;
}


hit virtualHit(hit a, hit b) {
	a.distance = -mix(abs(a.distance),abs(b.distance),.999);
	a.normal = vec3(.0);
	return a;
}


hit setUnion(hit a, hit b) {
	if(a.distance>=.0 && b.distance>=.0) return (a.distance<b.distance)?a:b;
	if(a.distance>=.0) return (-b.distance<a.distance)?b:virtualHit(a,b);
	if(b.distance>=.0) return (-a.distance<b.distance)?a:virtualHit(b,a);
	return (-a.distance<-b.distance)?virtualHit(a,b):virtualHit(b,a);
}

hit setNegate(hit h) {
	h.normal = -h.normal;
	h.distance = -h.distance;
	return h;
}

hit setIntersect(hit a, hit b) {
	return setNegate(setUnion(setNegate(a), setNegate(b))); // De Morgan really knew his shit
}

hit setDifference(hit a, hit b) {
	return setIntersect(a, setNegate(b));
}

uniform int zero;

hit cuboidObj(raycast ray) {
	hit a = sphere(ray, vec3(.0, .0, .0), .5*sqrt(2.), 2);
	for(int i = 0; i<6+zero; ++i) {
		vec3 o = vec3(.0);
		o[i%3] = 1.;
		if(i>=3) o = -o;
		a = setDifference(a, sphere(ray, o, .5*sqrt(2.)*.99, 2));
	}
	return a;
}

hit prism(raycast ray) {
	return setIntersect(
		plane(ray, vec3(.0, 1., .0), vec3(.0, 2.5, .0), 2),
		setIntersect(
			plane(ray, vec3(.0, -1., .0), vec3(.0, -.65, .0), 2),
			setIntersect(
				plane(ray, vec3(1., .0, .0), vec3(1.9, .0, .0), 2),
			setIntersect(
				plane(ray, vec3(.0, .0, 1.), vec3(1.4, .0, .5), 2),
				plane(ray, normalize(vec3(-.8, .0, -1.0)), vec3(.9, .0, -.5), 2)
	))));
}

hit scene2(raycast ray) {
	return
		setUnion(
			setUnion(
			sphere(ray, vec3(4.5, .0, 7.0), .5, 1),
				setUnion(
					sphere(ray, vec3(1.4-1.8, .0, 3.5+.5), 2.5, 3),
					setUnion(
						sphere(ray, vec3(2.9+1.9, .0, 3.5-.5), 2.5, 3),
						setUnion(
							sphere(ray, vec3(2.9, 3.0, 3.5-.5), 2.5, 3),
							sphere(ray, vec3(6.9+2., .0, 3.5-.5), 2.5, 3)
						)
					)
				)
			),
			setUnion(
				setUnion(
					plane(ray, vec3(.0, 1.0, .0), vec3(.0, -.75, .0), 3),
					plane(ray, vec3(.0, .0, 1.0), vec3(.0, .0, -2.0), 0)),
				prism(ray)
				/*setDifference(
					sphere(ray, vec3(.0, .0, .0), .4*sqrt(2.), 2),
					cuboidObj(ray)
				)*/
			)
		);
}

hit scene(raycast ray) {
	return
		setUnion(
			plane(ray, vec3(.0, 1.0, .0), vec3(.0, .0, .0), 0),
			setUnion(
				setIntersect(
					sphere(ray, vec3(-.25, .5, .5), .5, 2),
					sphere(ray, vec3(.25, .5, .5), .5, 2)
				),
				setUnion(
					plane(ray, vec3(-1., .0, .0), vec3(1., .0, .0), 1),
					plane(ray, vec3(1., .0, .0), vec3(-1., .0, .0), 0)
				)
			)
		);
}

struct material {
	vec3 diffuseAlbedo, specularAlbedo, emission;
	float ior;
	bool isDirac, transmit;
};

uniform sampler2D emission;

float wavelength;
material getMaterial(hit h, raycast ray) {
	vec3 p = ray.origin + h.distance*ray.direction;
	material m;
	switch(h.material) {
		case 0:
		m.diffuseAlbedo = vec3(.5, .5, .5);
		m.specularAlbedo = vec3(.0);
		m.emission = vec3(.0);
		m.isDirac = m.transmit = false;
		m.ior = 1.;
		break;
		case 1:
		m.diffuseAlbedo = vec3(1.0);
		m.specularAlbedo = vec3(.0);
		float aspect = float(textureSize(emission, 0).x)/float(textureSize(emission, 0).y);
		vec2 uv = vec2(.0,1.0)-vec2(-1.,1.)*clamp(vec2(.0,1.2)+p.zy*vec2(1., -aspect), vec2(.0), vec2(1.));
		m.emission = vec3(30.)*texture(emission, uv).xyz;
		
		//m.emission = vec3(4000.0);
		m.isDirac = m.transmit = false;
		m.ior = 1.;
		break;
		case 2:
		m.diffuseAlbedo = vec3(1.0);
		m.specularAlbedo = vec3(1.);
		m.emission = vec3(.0);
		m.isDirac = true;
		m.transmit = true;
		m.ior = (dot(h.normal, ray.direction)>=.0)?1.:mix(1.7, 1.5, (wavelength-350.)/400.);
		break;
		case 3:
		m.diffuseAlbedo = vec3(.2, .2, .2);
		m.specularAlbedo = vec3(.0);
		m.emission = vec3(.0);
		m.isDirac = m.transmit = false;
		m.ior = 1.;
		break;

	}
	return m;
}

int error = 0;

hit traceScene(raycast ray) {
	hit h;
	float totalDistance = .0;
	int k = 0;
	do {
		h = scene(ray);
		h.distance = abs(h.distance);
		totalDistance += h.distance;
		if(isnan(totalDistance) || isinf(totalDistance)) totalDistance = far;
		ray.origin += h.distance * ray.direction;
	} while(dot(h.normal, h.normal)==.0 && totalDistance<far && k++<40);
	if(k==20) error = 1;
	h.distance = totalDistance;
	return h;
}

void main() {
	const uint N = color.length();
	if(gl_GlobalInvocationID.x>=N) return;

	srand(tick, gl_GlobalInvocationID.x, 1);
	
	vec2 chi = vec2(rand(), rand());
	coords[gl_GlobalInvocationID.x] = chi;

	vec2 uv = chi*2.-vec2(1.);
	uv.x *= float(screenSize.x)/float(screenSize.y);

	wavelength = 400.+rand()*300.;
	float fov = 1.;

	raycast ray;
	ray.origin = vec3(.0, .0, .0);
	ray.direction = normalize(vec3(uv, -1./tan(fov*.5)));
	
	chi = vec2(rand(), 2.*pi*rand());
	uv = sqrt(chi.x)*vec2(cos(chi.y), sin(chi.y));
	
	ray.origin.xy = uv*.002;
	ray.direction = normalize(ray.direction*2.3-ray.origin);

	ray.origin = (cameraToWorld * vec4(ray.origin, 1.)).xyz;
	ray.direction = mat3(cameraToWorld) * ray.direction;

	hit u = traceScene(ray);
	
	vec3 spec = spectrum(wavelength);
	vec3 acSpec = vec3(1.);

	vec3 result = vec3(.1, .1, .2);
	if(u.distance<far) {
		float ior = 1.;
		material m = getMaterial(u, ray);
		result = m.emission;
		vec3 throughput = m.diffuseAlbedo;
		for(int i = 0; i<8+zero; ++i) {
		
			/*vec3 lightDirection = normalize(vec3(2., 4., 1.));
			raycast shadowRay; shadowRay.origin = ray.origin + .999*u.distance*ray.direction; shadowRay.direction = lightDirection;
			if(traceScene(shadowRay).distance>=far) result += throughput * .2 * max(dot(lightDirection, u.normal), .0);*/
			
			if(m.isDirac) {
				float R0 = (ior-m.ior)/(ior+m.ior);
				R0 *= R0;
				float fresnel = R0 + (1.-R0)*pow(max(.0, 1.-abs(dot(-ray.direction, u.normal))), 5.);
				if(m.transmit && fresnel<rand()) {
					acSpec = spec;
					if(dot(u.normal, ray.direction)>=.0) {
						u.normal = -u.normal;
						throughput *= exp(-u.distance*4.0);
					}
					float eta = ior/m.ior;
					float nl = dot(u.normal, -ray.direction);
					float T = 1.-eta*eta*(1.-nl*nl);
					if(T>=.0) {
						ray.origin = ray.origin + 1.0001*u.distance*ray.direction;
						ray.direction = normalize((eta*nl-sqrt(T))*u.normal + eta * ray.direction);
						ior = m.ior;
					} else {
						ray.origin = ray.origin + .9999*u.distance*ray.direction;
						ray.direction = reflect(ray.direction, u.normal);
					}
				} else {
					ray.origin = ray.origin + .999*u.distance*ray.direction;
					ray.direction = reflect(ray.direction, u.normal);
					if(!m.transmit)
						throughput *= fresnel;
				}
			} else {
				ray.origin = ray.origin + .9999*u.distance*ray.direction;
				chi = vec2(rand(), 2.*pi*rand());
				vec3 l; l.xy = sqrt(chi.x)*vec2(cos(chi.y), sin(chi.y));
				l.z = sqrt(max(.0, 1.-dot(l.xy, l.xy)));
				ray.direction = basis(u.normal) * l;
			}
			u = traceScene(ray);
			if(u.distance>=far) {
				result += vec3(.1, .1, .2)*throughput;
				break;
			}
			
			m = getMaterial(u, ray);
			result += throughput*m.emission;
			if(!m.isDirac)
				throughput *= m.diffuseAlbedo;
			else
				throughput *= m.specularAlbedo;

			if(dot(throughput, throughput) == .0) break;
		}
	}
	color[gl_GlobalInvocationID.x] =  vec4(result*acSpec, 1.);
}
