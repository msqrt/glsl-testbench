
#version 450

layout (local_size_x = 16, local_size_y = 16) in;

uniform uint frame;
uniform float t;
layout(rgba32f)uniform image2D result;
uniform mat4 cameraToWorld;

uvec4 rndseed;
void jenkins_mix()
{
	rndseed.x -= rndseed.y; rndseed.x -= rndseed.z; rndseed.x ^= rndseed.z>>13;
	rndseed.y -= rndseed.z; rndseed.y -= rndseed.x; rndseed.y ^= rndseed.x<<8;
	rndseed.z -= rndseed.x; rndseed.z -= rndseed.y; rndseed.z ^= rndseed.y>>13;
	rndseed.x -= rndseed.y; rndseed.x -= rndseed.z; rndseed.x ^= rndseed.z>>12;
	rndseed.y -= rndseed.z; rndseed.y -= rndseed.x; rndseed.y ^= rndseed.x<<16;
	rndseed.z -= rndseed.x; rndseed.z -= rndseed.y; rndseed.z ^= rndseed.y>>5;
	rndseed.x -= rndseed.y; rndseed.x -= rndseed.z; rndseed.x ^= rndseed.z>>3;
	rndseed.y -= rndseed.z; rndseed.y -= rndseed.x; rndseed.y ^= rndseed.x<<10;
	rndseed.z -= rndseed.x; rndseed.z -= rndseed.y; rndseed.z ^= rndseed.y>>15;
}
void srand(uint A, uint B, uint C) {rndseed = uvec4(A, B, C, 0); jenkins_mix();}

float rand()
{
	if(0==rndseed.w++%3) jenkins_mix();
	rndseed.xyz = rndseed.yzx;
	return float(rndseed.x)/pow(2.,32.);
}

// assumption: x positive
float copy_sign(float x, float sgn)
{
	return uintBitsToFloat(floatBitsToUint(x)|((1<<31)&floatBitsToUint(sgn)));
}

const float pi = 3.14159265358979;
mat3 basis(vec3 n)
{
    float sg = copy_sign(1., n.z);
    float a = -1.0 / (sg + n.z);
    float b = n.x * n.y * a;
    return mat3(vec3(1.0 + sg * n.x * n.x * a, sg * b, -sg * n.x), vec3(b, sg + n.y * n.y * a, -n.y), n);
}

const float far = 3.e38;

struct raycast
{
	vec3 origin, direction;
};

struct material
{
	vec3 diffuse_albedo, specular_albedo, emission;
	float glossiness;
};

struct hit
{
	material mat;
	vec3 normal;
	float distance;
};

material gen_material(vec3 diffuse_albedo, vec3 specular_albedo, vec3 emission, float glossiness)
{
	material m;
	m.diffuse_albedo = diffuse_albedo;
	m.specular_albedo = specular_albedo;
	m.emission = emission;
	m.glossiness = glossiness;
	return m;
}

mat4 transform(vec3 transform, float rotation, vec3 scale)
{
	mat4 m = mat4(1.);
	m[3].xyz = transform;
	m[0][0] = cos(rotation)*scale.x;
	m[2][0] = -sin(rotation)*scale.z;
	m[1][1] = scale.y;
	m[0][2] = sin(rotation)*scale.x;
	m[2][2] = cos(rotation)*scale.z;
	return inverse(m);
}

raycast transform_to_object(raycast r, mat4 m)
{
	r.origin = vec4(m * vec4(r.origin, 1.0)).xyz;
	r.direction = vec4(m * vec4(r.direction, .0)).xyz;
	return r;
}

hit transform_to_world(hit h, mat4 m)
{
	h.normal = vec4(transpose(m) * vec4(h.normal,.0)).xyz;
	return h;
}

hit plane(raycast ray)
{
	hit h;
	h.distance = -1.;
	if(ray.direction.y<.0) {
		h.distance = -ray.origin.y/ray.direction.y;
		h.normal = vec3(.0, 1., .0);
	}
	return h;
}

hit sphere(raycast ray, mat4 m, material mat)
{
	ray = transform_to_object(ray, m);
	hit h;
	h.mat = mat;
	h.distance = -1.;
	float a = dot(ray.direction, ray.direction), b = 2.*dot(ray.direction, ray.origin), c = dot(ray.origin, ray.origin)-1.;
	if(abs(a)<1e-8)
		h.distance = -c/b;
	else
	{
		float d = b*b-4.*a*c;
		if(d>=.0)
		{
			d = sqrt(d);
			h.distance = .5*(-b-d)/a;
			if(h.distance<.0) h.distance += d/a;
		}
		else return h;
	}
	h.normal = normalize(ray.origin+h.distance*ray.direction);
	return transform_to_world(h, m);
}

hit aabb(raycast ray, mat4 m, material mat)
{
	ray = transform_to_object(ray, m);
	hit h;
	h.mat = mat;
	h.normal = vec3(.0);
	h.distance = -1.;
	vec3 inv_direction = 1./ray.direction;

	vec3 mid = -ray.origin*inv_direction;
	
	vec3 enter = mid-abs(inv_direction), exit = mid+abs(inv_direction);
	float t0 = max(enter.x, max(enter.y, enter.z)), t1 = min(exit.x, min(exit.y, exit.z));
	if(t0<t1 && t1>=.0)
	{
		if(t0<.0)
		{
			t0 = t1;
			enter = -exit;
		}
		h.distance = t0;
		if(enter.x>enter.y&&enter.x>enter.z)
			h.normal[0] = copy_sign(1., -inv_direction[0]);
		else if(enter.y>enter.z)
			h.normal[1] = copy_sign(1., -inv_direction[1]);
		else
			h.normal[2] = copy_sign(1., -inv_direction[2]);
	}
	
	return transform_to_world(h, m);
}

hit combine(hit a, hit b)
{
	if(b.distance<.0) return a;
	if(a.distance<.0) return b;
	if(b.distance<a.distance) return b; else return a;
}

vec3 eval_brdf(material mat, vec3 normal, vec3 incident, vec3 exitant)
{
	return mat.diffuse_albedo/pi;
}

hit traceScene(raycast ray)
{
	hit h;

	h = combine(
			combine(
				combine(
					aabb(ray, transform(vec3(.0, 2.5, .0), .0, vec3(3., 2.5, 3.)), gen_material(vec3(.9), vec3(.1), vec3(.0), 20.)),
					aabb(ray, transform(vec3(.0, 5., .0), .0, vec3(.5, .001, .5)), gen_material(vec3(.0), vec3(.0), vec3(50.), 1.))
				),
				combine(
					aabb(ray, transform(vec3(5.98, 2.5, .0), .0, vec3(3., 2.5, 3.)), gen_material(vec3(.9, .2, .2), vec3(.1), vec3(.0), 20.)),
					aabb(ray, transform(vec3(-5.98, 2.5, .0), .0, vec3(3., 2.5, 3.)), gen_material(vec3(.2, .9, .2), vec3(.1), vec3(.0), 20.))
				)
			),
			combine(
				aabb(ray, transform(vec3(-1.0, 1.5, -1.0), .2, vec3(.75, 1.5, .75)), gen_material(vec3(.9), vec3(.1), vec3(.0), 60.)),
				combine(
					aabb(ray, transform(vec3(1.0, .75, 1.5), .3, vec3(.75)), gen_material(vec3(.9), vec3(.1), vec3(.0), 40.)),
					sphere(ray, transform(vec3(1.0, 2., 1.5), .0, vec3(.4, .5, .4)), gen_material(vec3(.0), vec3(1.), vec3(.0), 200.))
				)
			)
		);

	h.normal = normalize(h.normal);
	return h;
}

void main()
{
	const uvec2 size = imageSize(result).xy;
	if(gl_GlobalInvocationID.x>=size.x||gl_GlobalInvocationID.y>=size.y) return;

	srand(frame+1, gl_GlobalInvocationID.x+1, gl_GlobalInvocationID.y+1);
	
	vec2 chi = (vec2(gl_GlobalInvocationID.xy)+vec2(rand(), rand()))/vec2(size);

	vec2 uv = chi*2.-vec2(1.);
	uv.x *= float(size.x)/float(size.y);

	float fov = 1.2;

	raycast ray;
	ray.origin = vec3(.0, .0, .0);
	ray.direction = normalize(vec3(uv, -1./tan(fov*.5)));
	
	chi = vec2(rand(), 2.*pi*rand());
	uv = sqrt(chi.x)*vec2(cos(chi.y), sin(chi.y));
	
	ray.origin.xy = uv*.005;
	ray.direction = normalize(ray.direction*2.3-ray.origin);

	ray.origin = (cameraToWorld * vec4(ray.origin, 1.)).xyz;
	ray.direction = mat3(cameraToWorld) * ray.direction;

	hit h = traceScene(ray);
	
	vec3 color = vec3(.0);//
	
	if(h.distance<.0)
		color = vec3(.01);
	else
	{
		color += h.mat.emission;
		raycast shadowRay;
		shadowRay.origin = ray.origin+.995*h.distance*ray.direction;
		shadowRay.direction = normalize(vec3(rand()-.5, 5., rand()-.5)-shadowRay.origin);
		hit h2 = traceScene(shadowRay);
		if(h2.distance>=.0 && abs((shadowRay.origin+h2.distance*shadowRay.direction).y-4.99)<.01)
			color += h2.mat.emission * eval_brdf(h.mat, h2.normal, -ray.direction, shadowRay.direction) * max(.0, dot(shadowRay.direction, h.normal))* max(.0, -h2.normal.y) / (h2.distance*h2.distance);
	}
	imageStore(result, ivec2(gl_GlobalInvocationID.xy), vec4((imageLoad(result, ivec2(gl_GlobalInvocationID.xy)).xyz*float(frame)+color)/float(frame+1), .0));
}
