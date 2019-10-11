
#version 460

uniform sampler2D albedo, normal, depth, texcoord;

in vec2 uv;
out vec3 color;

void main() {
	vec2 coord = uv * .5 + vec2(.5);
	
	if (coord.x < 1. / 4.)
		color = vec3(abs(-.005 / abs(texture(depth, coord).x-1.)));
	else if (coord.x < 2. / 4.)
		color = texture(texcoord, coord).xyy; // color = texture(normal, coord).xyz*.5+vec3(.5);
	else if (coord.x < 3. / 4.)
		color = texture(texcoord, coord).xyz;
	else
		color = texture(albedo, coord).xyz;
}
