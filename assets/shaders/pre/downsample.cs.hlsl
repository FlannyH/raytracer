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

template <typename T>
void downsample_2d(uint x, uint y) {
    Texture2D<T> base_tex = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.base_tex & MASK_ID)];
    RWTexture2D<T> target_tex = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.target_tex & MASK_ID)];
    const T fetch1 = base_tex[uint2(x*2 + 0, y*2 + 0)];
    const T fetch2 = base_tex[uint2(x*2 + 1, y*2 + 0)];
    const T fetch3 = base_tex[uint2(x*2 + 0, y*2 + 1)];
    const T fetch4 = base_tex[uint2(x*2 + 1, y*2 + 1)];
    target_tex[uint2(x, y)] = (fetch1 + fetch2 + fetch3 + fetch4) / 4.0f;
}

template <typename T>
void downsample_cube(uint x, uint y, uint face) {
    Texture2DArray<T> base_tex = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.base_tex & MASK_ID)];
    RWTexture2DArray<T> target_tex = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.target_tex & MASK_ID)];
    const T fetch1 = base_tex[uint3(x*2 + 0, y*2 + 0, face)];
    const T fetch2 = base_tex[uint3(x*2 + 1, y*2 + 0, face)];
    const T fetch3 = base_tex[uint3(x*2 + 0, y*2 + 1, face)];
    const T fetch4 = base_tex[uint3(x*2 + 1, y*2 + 1, face)];
    target_tex[uint3(x, y, face)] = (fetch1 + fetch2 + fetch3 + fetch4) / 4.0f;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    const uint x = dispatch_thread_id.x;
    const uint y = dispatch_thread_id.y;
    const uint face = dispatch_thread_id.z;
    const uint n_components = root_constants.n_components;

    if (x >= root_constants.target_width) return;
    if (y >= root_constants.target_height) return;

    if (root_constants.is_cubemap) {
        if (n_components == 1) { downsample_cube<float>(x, y, face); }
        else if (n_components == 2) { downsample_cube<float2>(x, y, face); }
        else if (n_components == 3) { downsample_cube<float3>(x, y, face); }
        else if (n_components == 4) { downsample_cube<float4>(x, y, face); }
    }
    else {
        if (n_components == 1) { downsample_2d<float>(x, y); }
        else if (n_components == 2) { downsample_2d<float2>(x, y); }
        else if (n_components == 3) { downsample_2d<float3>(x, y); }
        else if (n_components == 4) { downsample_2d<float4>(x, y); }
    }
}
