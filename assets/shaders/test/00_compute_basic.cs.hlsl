struct RootConstants {
    uint output_buffer;
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

#define MASK_ID ((1 << 27) - 1)
#define MASK_IS_LOADED (1 << 27)
#define PI 3.14159265358979f

[numthreads(64, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    RWByteAddressBuffer output_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.output_buffer & MASK_ID)];
    for (uint i = 0; i < 512; i += 64) {
        uint index = i + dispatch_thread_id.x;
        output_buffer.Store(index * 4, index);
    }
}
