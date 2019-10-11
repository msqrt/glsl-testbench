
#version 460

#extension GL_KHR_shader_subgroup_basic: enable
#extension GL_KHR_shader_subgroup_ballot: enable
#extension GL_KHR_shader_subgroup_shuffle: enable
#extension GL_KHR_shader_subgroup_arithmetic: enable
#extension GL_ARB_gpu_shader_int64: enable

layout(local_size_x = 256) in;

layout(std430) buffer inputBuffer{uvec4 data[];};
layout(std430) buffer outputBuffer{uvec4 result[];};

// try: signed int everything

const uint local_size = 4u*gl_WorkGroupSize.x;

const uint subgroup_size = 32; // pls make match gl_SubgroupSize
shared uint local_store[(local_size/subgroup_size)*(subgroup_size+1u)];

// transfers val to a bitfield where every 4 bits of the uint64 represent a bin (for 64/4=16 bins)
uint64_t value_to_bitfield(const uint shift, const uint val)
{
	// negative shift not allowed so special cases for 0, 1
	if (shift == 0u)
		return 1ul << (4u * (val & 15u));
	else if (shift == 1u)
		return 1ul << (2u * (val & 30u));
	else
		return 1ul << (((val >> (shift-2)) & 60u));
}

// this is just a widening operation; bitfield contains 4 bits per bin (for max value 15), bins contains 10 for max value 1023
uint[5u] bitfield_to_bins(uint64_t bitfield)
{
	uint bins[5u];
	for (uint i = 0u; i < 4u; ++i)
	{
		bins[i] = uint(bitfield) & 15u;
		bitfield >>= 4u;
		bins[i] |= (uint(bitfield) & 15u) << 10u;
		bitfield >>= 4u;
		bins[i] |= (uint(bitfield) & 15u) << 20u;
		bitfield >>= 4u;
	}
	return bins;
}

// returns how many values of vals are in 16 bins for this shift (three packed into each uint32 and the last one implicitly deduced)
uint[5u] count_bins(const uint shift, const uvec4 vals)
{
	return bitfield_to_bins(
		value_to_bitfield(shift, vals[0]) + value_to_bitfield(shift, vals[1]) +
		value_to_bitfield(shift, vals[2]) + value_to_bitfield(shift, vals[3])
	);
}

uvec4 read_local_linear() {
	uvec4 val;
	for(uint i = 0u; i<4u; ++i)
		val[i] = local_store[gl_LocalInvocationID.x*4u+i];
	return val;
}

const uint warps = gl_WorkGroupSize.x/subgroup_size;

uint avoid_conflict(uint x)
{
	return x + (x >> 5);
}

uint copy_total(uint tot)
{
	return tot + (tot << 10u) + (tot << 20u);
}

uvec4 sort(const uint shift, uvec4 val)
{
	uint bins[5u] = count_bins(shift, val);
	for(uint i = 0u; i<5u; ++i)
	{
		const uint tmp = subgroupExclusiveAdd(bins[i]);
		if(gl_SubgroupInvocationID==subgroup_size-1u)
			local_store[gl_SubgroupID+i*(warps+1u)] = tmp+bins[i];
		bins[i] = tmp;
	}
	barrier();

	if(gl_SubgroupInvocationID<warps+1 && gl_SubgroupID<5u)
	{
		uint tmp = local_store[gl_SubgroupID*(warps+1u)+gl_SubgroupInvocationID];
		tmp = subgroupExclusiveAdd(tmp);
		// the thread (gl_SubgroupInvocationID == warps) has the total value here
		if (gl_SubgroupInvocationID == warps)
			tmp = copy_total(tmp);
		local_store[gl_SubgroupID*(warps + 1) + gl_SubgroupInvocationID] = tmp;
	}
	barrier();
	uint last_bin = gl_LocalInvocationIndex * 4;
	uint prev_total = 0u;
	for (uint i = 0u; i < 5u; ++i) {
		uint total = local_store[warps + i * (warps + 1u)];
		bins[i] += local_store[gl_SubgroupID + i * (warps + 1u)];
		last_bin += bins[i];
		bins[i] += copy_total(prev_total) + (total << 10u);
		prev_total += total >> 20u;
	}
	last_bin += prev_total;
	barrier();
	for(uint i = 0u; i < 5u; ++i)
	for(uint j = 0u; j < 3u; ++j)
	{
		const uint bin_index = i * 3u + j;
		uint bin = (bins[i] >> (j * 10u)) & 1023u;
		for(uint k = 0u; k < 4u; ++k)
			if (((val[k] >> shift)&15u) == bin_index)
				local_store[avoid_conflict(bin++)] = val[k];
	}
	//for (uint k = 0u; k < 4u; ++k)
	//	if (((val[k] >> shift) & 15u) != 15)
	//		last_bin++;
	//for (uint k = 0u; k < 4u; ++k)
	//	if (((val[k] >> shift) & 15u) == 15)
	//		local_store[avoid_conflict(last_bin++)] = val[k];

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
