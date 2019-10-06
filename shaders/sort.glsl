
#version 460

#extension GL_KHR_shader_subgroup_basic: enable
#extension GL_KHR_shader_subgroup_ballot: enable
#extension GL_KHR_shader_subgroup_shuffle: enable
#extension GL_KHR_shader_subgroup_arithmetic: enable
#extension GL_ARB_gpu_shader_int64: enable

layout(local_size_x = 256) in;

layout(std430) buffer inputBuffer{uvec4 data[];};
layout(std430) buffer outputBuffer{uvec4 result[];};

const uint local_size = 4u*gl_WorkGroupSize.x;

const uint subgroup_size = 32; // pls make match gl_SubgroupSize
shared uint local_store[(local_size/subgroup_size)*(subgroup_size+1u)];

// transfers val to a bitfield where every 4 bits of the uint64 represent a bin (for 64/4=16 bins)
uint64_t bin_to_bitfield(const uint shift, const uint val)
{
	// negative shift not allowed so special cases for 0, 1
	if (shift == 0u)
		return 1ul << (4u * (val & 15u));
	else if (shift == 1u)
		return 1ul << (2u * (val & 30u));
	else
		return 1ul << (((val >> (shift-2)) & 60u));
}

// returns an array of counts of val in 16 bins (two packed into each uint32)
uint[5u] count_bins(const uint shift, const uvec4 val)
{
	// add the elements
	uint64_t bitfield = 
		bin_to_bitfield(shift, val[0]) + bin_to_bitfield(shift, val[1]) +
		bin_to_bitfield(shift, val[2]) + bin_to_bitfield(shift, val[3]);

	// this is just a widening operation; bitfield contains 4 bits per bin (for max value 15), bins contains 10 for max value 1023
	uint bins[5u];
	for(uint i = 0u; i<4u; ++i)
	{
		bins[i] = uint(bitfield)&15u;
		bitfield >>= 4u;
		bins[i] |= (uint(bitfield)&15u) << 10u;
		bitfield >>= 4u;
		bins[i] |= (uint(bitfield) & 15u) << 20u;
		bitfield >>= 4u;
	}

	return bins;
}

uvec4 local_linear() {
	uvec4 val;
	for(uint i = 0u; i<4u; ++i)
		val[i] = local_store[gl_LocalInvocationID.x*4u+i];
	return val;
}

const uint warps = gl_WorkGroupSize.x/subgroup_size;

uvec4 sort(const uint shift, uvec4 val)
{
	uint bins[5u] = count_bins(shift, val);
	for(uint i = 0u; i<5u; ++i)
	{
		const uint tmp = subgroupExclusiveAdd(bins[i]);
		if(gl_SubgroupInvocationID==subgroup_size-1)
			local_store[gl_SubgroupID+i*warps] = tmp+bins[i];
		bins[i] = tmp;
	}
	barrier();

	if(gl_SubgroupInvocationID<5u)
	{
		const uint tmp = local_store[gl_SubgroupID*5u+gl_SubgroupInvocationID];
		local_store[gl_SubgroupID*5u+gl_SubgroupInvocationID] = subgroupExclusiveAdd(tmp);
	}
	barrier();

	uint sum_last = 0u;
	for(uint i = 0u; i<16u; ++i)
	{
		
	}

	barrier();

	return uvec4(bins[0]+bins[1]+bins[2]+bins[3]+bins[4]);
}

void main()
{
	for(uint i = gl_GlobalInvocationID.x; i<data.length(); i+=gl_WorkGroupSize.x*gl_NumWorkGroups.x)
	{
		result[i] = sort(0, data[i]);
	}
}
