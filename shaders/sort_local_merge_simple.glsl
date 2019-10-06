
#version 460

#extension GL_KHR_shader_subgroup_basic: enable
#extension GL_KHR_shader_subgroup_ballot: enable
#extension GL_KHR_shader_subgroup_shuffle: enable
#extension GL_KHR_shader_subgroup_arithmetic: enable

layout(local_size_x = 256) in;

layout(std430) buffer inputBuffer{uvec4 data[];};
layout(std430) buffer outputBuffer{uvec4 result[];};

const uint local_size = 4u*gl_WorkGroupSize.x;
shared uint local_store[local_size];

uvec4 local_sort(uvec4 val)
{
	if(val.x>val.z) val.xz = val.zx;
	if(val.y>val.w) val.yw = val.wy;
	if(val.x>val.y) val.xy = val.yx;
	if(val.z>val.w) val.zw = val.wz;
	if(val.y>val.z) val.yz = val.zy;
	return val;
}

uvec4 pair_merge(uvec4 val)
{
	if(val.x>val.y) val.xy = val.yx;
	if(val.z>val.w) val.zw = val.wz;
	return val;
}

uvec4 local_merge(uvec4 val)
{
	if(val.x>val.z) val.xz = val.zx;
	if(val.y>val.w) val.yw = val.wy;
	return val;
}

uvec4 ballot_merge(const uint size, uvec4 val)
{
	const uvec4 other = subgroupShuffleXor(val, size);
	const uint select = gl_SubgroupInvocationID&size;
	for(uint i = 0u; i<4u; ++i)
		if((val[i]<other[i])^^(select==0))
			val[i] = other[i];
	return val;
}

uvec4 shared_merge(const uint size, uvec4 val)
{
	barrier();
	for(int i = 0; i<4; ++i)
		local_store[gl_LocalInvocationID.x+i*gl_WorkGroupSize.x] = val[i];

	const uint select = gl_LocalInvocationID.x&size;
	const uint otherID = gl_LocalInvocationID.x^size;
	
	barrier();
	for(int i = 0; i<4; ++i)
	{
		const uint other = local_store[otherID+i*gl_WorkGroupSize.x];
		if((val[i]<other)^^(select==0))
			val[i] = other;
	}
	return val;
}

uvec4 merge(const uint size, uvec4 val)
{
	for(uint i = size>>3u; i!=0u; i>>=1u)
		if(i>=gl_SubgroupSize)
			val = shared_merge(i, val);
		else
			val = ballot_merge(i, val);

	//if(size>=4)
		val = local_merge(val);

	return pair_merge(val);
}

uvec4 merge_sort(uvec4 val)
{
	const uint all_bits = 0xFFFFFFFF;
	{
		const uint select = all_bits*(gl_SubgroupInvocationID.x&1u);
		val = select^local_sort(select^val);
	}
	for(uint size = 8u; size<=local_size; size <<= 1u)
	{
		const uint select = ((gl_LocalInvocationID.x&(size>>2u)) == 0) ? 0 : all_bits;
		val = select^merge(size, select^val);
	}
	return val;
}

void main()
{
	for(uint i = gl_GlobalInvocationID.x; i<data.length(); i+=gl_WorkGroupSize.x*gl_NumWorkGroups.x)
	{
		result[i] = merge_sort(data[i]);
	}
}
