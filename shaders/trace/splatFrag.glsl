
#version 450

in vec4 value;
in vec2 uv;

float mitchell_netravali(float x) {
	const float B = 0.8, C = 0.1;
	if(x<=1.) return ((12.-9.*B-6.*C)*x*x*x+(-18.+12.*B+6.*C)*x*x+(6.-2.*B));
	if(x<=2.) return ((-B-6.*C)*x*x*x+(6.*B+30.*C)*x*x+(-12.*B-48.*C)*x+(8.*B+24.*C));
	return .0;
}

out vec4 color;

void main() {
	color = value*mitchell_netravali(length(uv));
}
