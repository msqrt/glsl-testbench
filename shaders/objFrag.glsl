
#version 460

in vec3 albedo, position, normal_varying;
out vec3 color, normal;

uniform sampler2D logo;

void main() {
	color = vec3(1.)-texture(logo, vec2(.0,1.)+vec2(1.,-1.)*clamp(position.xy*vec2(-2.,2.)-vec2(-1.0,.1), vec2(.0), vec2(1.))).xyz;
	if (position.z < .55) color = vec3(1.);
	color *= albedo;
	normal = normal_varying;
}