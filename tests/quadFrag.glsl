
#version 450

layout(std430) buffer points {vec4 point[];};
layout(std430) buffer uvs {vec2 tcoord[];};
layout(std430) buffer normals {vec4 nor[];};
layout(std430) buffer faces {ivec4 face[];};

out vec3 color;
in vec3 p[4];
in vec3 ray;
in flat int quad;

bool traceBilinearPatch(vec3 ro, vec3 rd, vec3 pa, vec3 pb, vec3 pc, vec3 pd,
                        out vec3 outs, out vec3 outt, out vec3 outuvt)
{
    pb -= pa; pc -= pa; pd -= pa; ro -= pa;
    mat3 M = mat3(pb, pc, pd);
    if(abs(determinant(M))<1.e-7) {
        M[2] = cross(pb, pc);
        M = mat3(1.,.0,.0,.0,.0,1.,.0,1.,.0)*inverse(M);
   		ro = M * ro;
    	rd = M * rd;
        if(abs(rd.y)<1.e-7) return false;
        float t = -ro.y/rd.y;
        outuvt = vec3(ro.xz + t*rd.xz, t);
		vec3 pp = M * pd;
		float a = -(pp.z-1.), b = outuvt.x*(pp.z-1.)-outuvt.y*(pp.x-1.)-1., c = outuvt.x, d = b*b-4.*a*c, u;
		if(d<.0) return false;
		if(abs(a)>1.e-7) {
			a = .5/a;
			d = abs(a*sqrt(d));
			b = -b*a;
			u = b-d;
			if(u<.0||u>1.) u = b+d;
		}
		else
			u = -c/b;
		outuvt.xy = vec2(u, outuvt.y/(u*(pp.z-1.)+1.));
    }
    else {
 	   	M = mat3(1.,.0,.0,.0,.0,1.0,1.,-1.,1.)*inverse(M);
        ro = M * ro;
        rd = M * rd;

        float c = ro.y+ro.x*ro.z, b = rd.y+rd.x*ro.z+ro.x*rd.z, a = rd.x*rd.z, d = b*b-4.*a*c;
        if(d<.0) return false;
        float t0, t1;
        if(abs(a)>1.e-7) {
            a = .5/a;
            d = abs(a*sqrt(d));
            b = -b*a;
            t0 = b-d;
			t1 = b+d;
        }
        else
            t0 = t1 = -c/b;
        outuvt = vec3(ro.xz + t0*rd.xz, t0);
        if(t0!=t1 && (outuvt.z<.0 || outuvt.x<.0 || outuvt.y<.0 || outuvt.x>1. || outuvt.y>1.))
            outuvt = vec3(ro.xz + t1*rd.xz, t1);
    }
    //if(outuvt.z<.0 || outuvt.x<.0 || outuvt.y<.0 || outuvt.x>1. || outuvt.y>1.)
    //    return false;

    outs = pb * outuvt.x - mix(pc, pd, outuvt.x);
    outt = pc * outuvt.y - mix(pb, pd, outuvt.y);
	
    return true;
}

void main() {
	vec3 r = -ray/ray.z;
	vec3 s, t, uvt;
	if(!traceBilinearPatch(vec3(.0), r, p[0], p[1], p[2], p[3], s, t, uvt)) discard;
	gl_FragDepth = uvt.z;
	color = vec3(dot(normalize(cross(s, t)), normalize(vec3(-1.)))*.5+.5);
}
