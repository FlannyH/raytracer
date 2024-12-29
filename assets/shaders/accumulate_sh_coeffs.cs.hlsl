struct RootConstants {
    uint output_buffer; // buffer to store the reduced result
    uint input_buffer; // buffer to accumulate
    uint input_buffer_length; // number of elements in the buffer
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

struct SHCoefficients {
    float3 l00;
    float3 l11;
    float3 l10;
    float3 l1_1;
    float3 l21;
    float3 l2_1;
    float3 l2_2;
    float3 l20;
    float3 l22;
};
#define sizeof_shcoefficients (9 * 12)

#define MASK_ID ((1 << 27) - 1)
#define THREADGROUP_SIZE 128

groupshared SHCoefficients tg_shared_buffer[THREADGROUP_SIZE * 2];

[numthreads(THREADGROUP_SIZE, 1, 1)]
void main(uint3 group_id : SV_GroupID, uint3 thread_id : SV_DispatchThreadID) {
    // todo: does this need to be globally coherent?
    ByteAddressBuffer input_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.input_buffer & MASK_ID)];
    globallycoherent RWByteAddressBuffer output_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.output_buffer & MASK_ID)];
    
    uint local_thread_id = thread_id.x % THREADGROUP_SIZE;

    // Load the first half into the shared buffer
    uint src_offset1 = local_thread_id + group_id.x * (THREADGROUP_SIZE * 2);
    uint limit = root_constants.input_buffer_length;
    if (src_offset1 < root_constants.input_buffer_length) {
        SHCoefficients sh1 = input_buffer.Load<SHCoefficients>(src_offset1 * sizeof_shcoefficients);
        tg_shared_buffer[local_thread_id].l00 = sh1.l00;
        tg_shared_buffer[local_thread_id].l11 = sh1.l11;
        tg_shared_buffer[local_thread_id].l10 = sh1.l10;
        tg_shared_buffer[local_thread_id].l1_1 = sh1.l1_1;
        tg_shared_buffer[local_thread_id].l21 = sh1.l21;
        tg_shared_buffer[local_thread_id].l2_1 = sh1.l2_1;
        tg_shared_buffer[local_thread_id].l2_2 = sh1.l2_2;
        tg_shared_buffer[local_thread_id].l20 = sh1.l20;
        tg_shared_buffer[local_thread_id].l22 = sh1.l22;
    }
    else {
        tg_shared_buffer[local_thread_id].l00 = float3(0, 0, 0);
        tg_shared_buffer[local_thread_id].l11 = float3(0, 0, 0);
        tg_shared_buffer[local_thread_id].l10 = float3(0, 0, 0);
        tg_shared_buffer[local_thread_id].l1_1 = float3(0, 0, 0);
        tg_shared_buffer[local_thread_id].l21 = float3(0, 0, 0);
        tg_shared_buffer[local_thread_id].l2_1 = float3(0, 0, 0);
        tg_shared_buffer[local_thread_id].l2_2 = float3(0, 0, 0);
        tg_shared_buffer[local_thread_id].l20 = float3(0, 0, 0);
        tg_shared_buffer[local_thread_id].l22 = float3(0, 0, 0);
    }
    GroupMemoryBarrierWithGroupSync();

    // Load the second half into the shared buffer
    uint src_offset2 = src_offset1 + THREADGROUP_SIZE;
    if (src_offset2 < root_constants.input_buffer_length) {
        SHCoefficients sh2 = input_buffer.Load<SHCoefficients>(src_offset2 * sizeof_shcoefficients);
        tg_shared_buffer[local_thread_id + THREADGROUP_SIZE].l00 = sh2.l00;
        tg_shared_buffer[local_thread_id + THREADGROUP_SIZE].l11 = sh2.l11;
        tg_shared_buffer[local_thread_id + THREADGROUP_SIZE].l10 = sh2.l10;
        tg_shared_buffer[local_thread_id + THREADGROUP_SIZE].l1_1 = sh2.l1_1;
        tg_shared_buffer[local_thread_id + THREADGROUP_SIZE].l21 = sh2.l21;
        tg_shared_buffer[local_thread_id + THREADGROUP_SIZE].l2_1 = sh2.l2_1;
        tg_shared_buffer[local_thread_id + THREADGROUP_SIZE].l2_2 = sh2.l2_2;
        tg_shared_buffer[local_thread_id + THREADGROUP_SIZE].l20 = sh2.l20;
        tg_shared_buffer[local_thread_id + THREADGROUP_SIZE].l22 = sh2.l22;
    }
    else {
        tg_shared_buffer[local_thread_id + THREADGROUP_SIZE].l00 = float3(0, 0, 0);
        tg_shared_buffer[local_thread_id + THREADGROUP_SIZE].l11 = float3(0, 0, 0);
        tg_shared_buffer[local_thread_id + THREADGROUP_SIZE].l10 = float3(0, 0, 0);
        tg_shared_buffer[local_thread_id + THREADGROUP_SIZE].l1_1 = float3(0, 0, 0);
        tg_shared_buffer[local_thread_id + THREADGROUP_SIZE].l21 = float3(0, 0, 0);
        tg_shared_buffer[local_thread_id + THREADGROUP_SIZE].l2_1 = float3(0, 0, 0);
        tg_shared_buffer[local_thread_id + THREADGROUP_SIZE].l2_2 = float3(0, 0, 0);
        tg_shared_buffer[local_thread_id + THREADGROUP_SIZE].l20 = float3(0, 0, 0);
        tg_shared_buffer[local_thread_id + THREADGROUP_SIZE].l22 = float3(0, 0, 0);
    }
    GroupMemoryBarrierWithGroupSync();

    // Accumulate the two halves
    for (uint n = THREADGROUP_SIZE; n > 0; n >>= 1) {
        if (local_thread_id < n) {
            tg_shared_buffer[local_thread_id].l00  += tg_shared_buffer[local_thread_id + n].l00;
            tg_shared_buffer[local_thread_id].l11  += tg_shared_buffer[local_thread_id + n].l11;
            tg_shared_buffer[local_thread_id].l10  += tg_shared_buffer[local_thread_id + n].l10;
            tg_shared_buffer[local_thread_id].l1_1 += tg_shared_buffer[local_thread_id + n].l1_1;
            tg_shared_buffer[local_thread_id].l21  += tg_shared_buffer[local_thread_id + n].l21;
            tg_shared_buffer[local_thread_id].l2_1 += tg_shared_buffer[local_thread_id + n].l2_1;
            tg_shared_buffer[local_thread_id].l2_2 += tg_shared_buffer[local_thread_id + n].l2_2;
            tg_shared_buffer[local_thread_id].l20  += tg_shared_buffer[local_thread_id + n].l20;
            tg_shared_buffer[local_thread_id].l22  += tg_shared_buffer[local_thread_id + n].l22;
        }
        GroupMemoryBarrierWithGroupSync();
    }

    // Write the result to the output buffer
    if (local_thread_id == 0) {
        uint dst_offset = group_id.x * sizeof_shcoefficients;
        output_buffer.Store(dst_offset + 0, tg_shared_buffer[0].l00);
        output_buffer.Store(dst_offset + 12, tg_shared_buffer[0].l11);
        output_buffer.Store(dst_offset + 24, tg_shared_buffer[0].l10);
        output_buffer.Store(dst_offset + 36, tg_shared_buffer[0].l1_1);
        output_buffer.Store(dst_offset + 48, tg_shared_buffer[0].l21);
        output_buffer.Store(dst_offset + 60, tg_shared_buffer[0].l2_1);
        output_buffer.Store(dst_offset + 72, tg_shared_buffer[0].l2_2);
        output_buffer.Store(dst_offset + 84, tg_shared_buffer[0].l20);
        output_buffer.Store(dst_offset + 96, tg_shared_buffer[0].l22);
    }
    GroupMemoryBarrierWithGroupSync();
}
