
#version 460

#extension GL_KHR_shader_subgroup_basic: enable
#extension GL_KHR_shader_subgroup_ballot: enable
#extension GL_KHR_shader_subgroup_shuffle: enable
#extension GL_KHR_shader_subgroup_arithmetic: enable
#extension GL_ARB_gpu_shader_int64: enable

layout(local_size_x = 256) in;

layout(std430) buffer; // set as default

buffer inputBuffer{uvec4 data[];};
buffer outputBuffer{uvec4 result[];};

// try: signed int everything

const uint local_size = 4u*gl_WorkGroupSize.x;

const uint subgroup_size = 32; // pls make match gl_SubgroupSize
shared uint local_store[(local_size/subgroup_size)*(subgroup_size+1u)];

uint get_bin(const uint shift, const uint val) {
	return (val >> shift) & 3u;
}

uint value_to_bitfield(const uint shift, const uint val)
{
	return 1u << (10u * get_bin(shift, val));
}

uvec4 read_local_linear()
{
	uvec4 val;
	for(uint i = 0u; i<4u; ++i)
		val[i] = local_store[gl_LocalInvocationID.x*4u+i];
	return val;
}

const uint warps = gl_WorkGroupSize.x/subgroup_size;

uint avoid_conflict(const uint x)
{
	return x + (x >> 5u);
}

uvec4 sort(const uint shift, uvec4 val)
{
	uint bins = 0;
	if (gl_LocalInvocationID.x < gl_WorkGroupSize.x - 1)
		for (uint i = 0u; i < 4u; ++i)
			bins += value_to_bitfield(shift, val[i]);

	const uint tmp = subgroupExclusiveAdd(bins);
	if(gl_SubgroupInvocationID==subgroup_size-1u)
		local_store[gl_SubgroupID] = tmp+bins;
	bins = tmp;

	barrier();

	if(gl_SubgroupID == (warps - 1) && gl_SubgroupInvocationID >= subgroup_size-warps-1)
	{
		const uint addr = gl_SubgroupInvocationID - (subgroup_size - warps - 1);
		uint tmp = local_store[addr];
		tmp = subgroupExclusiveAdd(tmp);
		// the thread (gl_SubgroupInvocationID == subgroup_size-1) has the total value (except for its own bins)
		if (gl_SubgroupInvocationID == subgroup_size - 1u)
		{
			for (uint i = 0u; i < 3u; ++i)
				local_store[warps + i] = (tmp>>(10u*i))&1023u;
			for (uint i = 0u; i < 4u; ++i)
				local_store[warps + get_bin(shift, val)]++;
			uint acc = 0;
			for (uint i = 0u; i < 4u; ++i)
			{
				uint t = local_store[warps + i];
				local_store[warps + i] = acc;
				acc += t;
			}
		}
		local_store[addr] = tmp;
	}
	barrier();
	bins += local_store[gl_SubgroupID];

	uint last_bin = gl_LocalInvocationIndex * 4;
	for (uint i = 0u; i < 3u; ++i)
		last_bin -= (tmp >> (10u * i)) & 1023u;

	uint prev_total = 0u;
	for (uint k = 0u; k < 4u; ++k) {
		uint bin = (val[k] >> shift) & 3u;
		uint addr = last_bin + 
		local_store[avoid_conflict(addr)] = val[k];

	}
	barrier();

	for (uint i = 0u; i < 4u; ++i)
		val[i] = local_store[avoid_conflict(gl_LocalInvocationIndex * 4 + i)];

	barrier();
	return val;
}

void main()
{
	for(uint i = gl_GlobalInvocationID.x; i<data.length(); i+=gl_WorkGroupSize.x*gl_NumWorkGroups.x)
	{
		result[i] = sort(0, data[i]);
	}
}
