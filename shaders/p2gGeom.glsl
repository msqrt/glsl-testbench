
#version 450

layout(points) in;
layout(triangle_strip, max_vertices=4) out;

layout(std430) buffer posBuffer {vec2 particle_pos[];};
layout(std430) buffer velBuffer {vec2 particle_vel[];};
layout(std430) buffer affBuffer {vec4 affine[];};

out vec2 uv;
out vec2 vel;
out mat2 aff;
out flat int write;
out int gl_Layer;

uniform ivec2 res;

void main() {
	
	vec2 pos = particle_pos[gl_PrimitiveIDIn];
	vec2 velocity = particle_vel[gl_PrimitiveIDIn];
	vec4 aff_map = affine[gl_PrimitiveIDIn];

	ivec2 cell = ivec2(pos*vec2(res))-ivec2(1);

	//if(cell.x+3>=0 && cell.y+3>=0 && cell.x-1<res.x && cell.y-1<res.y) {
		for(int i = 0; i<4; ++i) {
			vel = velocity;
			aff = mat2(aff_map.xy, aff_map.zw);
			enablePrintf();
			uv = vec2(float(i%2), float(i/2));
			vec2 splat_pos = vec2(cell) + uv*3;
			//splat_pos += .5*(uv-vec2(.5));
			uv = (pos*vec2(res)-splat_pos);
			gl_Position = vec4(splat_pos/vec2(res)*2.-vec2(1.), .0, 1.0);
			gl_Layer = 0;
			EmitVertex();
		}
		EndPrimitive();
	//}
}
