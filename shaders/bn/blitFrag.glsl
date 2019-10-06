
#version 450

uniform sampler2D result, dist, meme;
out vec3 color;

void main()
{
	const uvec2 size = textureSize(result, 0);
	if(uint(gl_FragCoord.x)<size.x)
	{
		const float value = float(floatBitsToInt(texelFetch(result, ivec2(gl_FragCoord.xy), 0).x))/float(size.x*size.y);
		const float var = 1.-dot(vec3(1.), texture(meme, vec2(gl_FragCoord.xy)/vec2(size)*vec2(1.,-1.)).xyz)/3.;
		color = vec3(value>var?1.:.0);
	}
	else
	{
		const float value = texelFetch(dist, ivec2(gl_FragCoord.xy)-ivec2(size.x, 0), 0).x/length(vec2(size.xy));
		color = vec3(value);
	}
}
