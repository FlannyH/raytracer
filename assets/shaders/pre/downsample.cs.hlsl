struct RootConstants {
    uint base_tex;
    uint target_tex;
    uint target_width;
    uint target_height;
    uint n_components;
    uint is_cubemap;
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

#define MASK_ID ((1 << 27) - 1)
#define PI 3.14159265358979f

void downsample_2d(uint x, uint y) {
    Texture2D base_tex = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.base_tex & MASK_ID)];
    RWTexture2D<float4> target_tex = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.target_tex & MASK_ID)];
    const float4 fetch1 = base_tex[uint2(x*2 + 0, y*2 + 0)];
    const float4 fetch2 = base_tex[uint2(x*2 + 1, y*2 + 0)];
    const float4 fetch3 = base_tex[uint2(x*2 + 0, y*2 + 1)];
    const float4 fetch4 = base_tex[uint2(x*2 + 1, y*2 + 1)];

    target_tex[uint2(x, y)] = (fetch1 + fetch2 + fetch3 + fetch4) / 4.0f;
}

void downsample_cube(uint x, uint y, uint face) {
    Texture2DArray base_tex = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.base_tex & MASK_ID)];
    RWTexture2DArray<float4> target_tex = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.target_tex & MASK_ID)];
    const float4 fetch1 = base_tex[uint3(x*2 + 0, y*2 + 0, face)];
    const float4 fetch2 = base_tex[uint3(x*2 + 1, y*2 + 0, face)];
    const float4 fetch3 = base_tex[uint3(x*2 + 0, y*2 + 1, face)];
    const float4 fetch4 = base_tex[uint3(x*2 + 1, y*2 + 1, face)];
    target_tex[uint3(x, y, face)] = (fetch1 + fetch2 + fetch3 + fetch4) / 4.0f;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    const uint x = dispatch_thread_id.x;
    const uint y = dispatch_thread_id.y;
    const uint face = dispatch_thread_id.z;

    if (x >= root_constants.target_width) return;
    if (y >= root_constants.target_height) return;

    if (root_constants.is_cubemap) {
        downsample_cube(x, y, face);
    }
    else {
        downsample_2d(x, y);
    }
}
