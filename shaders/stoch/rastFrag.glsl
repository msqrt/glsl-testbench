
#version 450

out vec3 color;

in vec2 vel;

layout(r32ui) uniform uimage2D outBuff;

in vec4 begin, end;

void main() {
	ivec2 size = imageSize(outBuff);

	vec2 b = vec2(size)*(begin.xy/begin.w*.5+vec2(.5))+vec2(.5);
	vec2 e = vec2(size)*(end.xy/end.w*.5+vec2(.5))+vec2(.5);

	int steps = max(1,int(.5+length(b-e)));
	for(int i = 0; i<steps; ++i) {
		for(int j = 0; j<2; ++j) {
			for(int k = 0; k<2; ++k) {
				float t = float(i+1)/float(steps+1);
				ivec2 pixel = ivec2(mix(b,e,t)-vec2(.5))+ivec2(j,k);
			
				float tt = .0;
				ivec2 frag = pixel;
				float scale = 1.;
				for(int i = 0; i<int(log2(2560)); ++i) {
					ivec2 local = frag%2;
					int ind = local.x+2*local.y;
					scale *= .25;
					switch(ind) {
						case 1: tt += scale*3; break;
						case 2: tt += scale*2; break;
						case 3: tt += scale; break;
					}
					frag /= 2;
				}
				if(length(vec2(pixel)+vec2(.5)-mix(b,e,tt))<1.)
					imageAtomicMax(outBuff, pixel, 1u);
			}
		}
	}
	discard;
}
