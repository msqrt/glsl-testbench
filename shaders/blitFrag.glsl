
#version 460

uniform sampler2D albedo, normal, depth;

in vec2 uv;
out vec3 color;

void main() {
	vec2 coord = uv * .5 + vec2(.5);
	if (coord.x < 1. / 3.)
		color = vec3(exp(-2. / abs(texture(depth, coord).x)));
	else if (coord.x < 2. / 3.)
		color = texture(normal, coord).xyz;
	else
		color = texture(albedo, coord).xyz;
}
