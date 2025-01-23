struct RootConstants {
    uint hdri;
    uint cube_map;
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

#define MASK_ID ((1 << 27) - 1)
#define MASK_IS_LOADED (1 << 27)
#define PI 3.14159265358979f

sampler tex_sampler : register(s0);

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    Texture2D<float4> hdri = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.hdri & MASK_ID)];
    RWTexture2DArray<float4> cubemap = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.cube_map & MASK_ID)];

    float cubemap_w, cubemap_h, cubemap_faces;
    cubemap.GetDimensions(cubemap_w, cubemap_h, cubemap_faces);

    uint dst_x = dispatch_thread_id.x;
    uint dst_y = dispatch_thread_id.y;
    uint face = dispatch_thread_id.z;

    float u = ((float(dst_x) + 0.5f) / float(cubemap_w)) * 2.0f - 1.0f;
    float v = ((float(dst_y) + 0.5f) / float(cubemap_h)) * 2.0f - 1.0f;

    float3 dir = float3(0.0f, 0.0f, 0.0f);
    switch (face) {
    case 0: dir = normalize(float3(1.0f, v, u));   break;
    case 1: dir = normalize(float3(-1.0f, v, -u)); break;
    case 2: dir = normalize(float3(u, -1.0f, -v)); break;
    case 3: dir = normalize(float3(u, 1.0f, v));   break;
    case 4: dir = normalize(float3(u, v, -1.0f));  break;
    case 5: dir = normalize(float3(-u, v, 1.0f));  break;
    }

    float spherical_u = atan2(dir.z, dir.x) / (2.0f * PI) + 0.5f;
    float spherical_v = asin(dir.y) / PI + 0.5f;

    float4 pixel = hdri.SampleLevel(tex_sampler, float2(1.0 - spherical_u, spherical_v), 0);
    pixel.r = min(pixel.r, 16000.0f); // some HDRIs have very high values
    pixel.g = min(pixel.g, 16000.0f); // so to avoid blowing out the values
    pixel.b = min(pixel.b, 16000.0f); // let's clamp them to a reasonable limit
    
    cubemap[uint3(dst_x, dst_y, face)] = pixel;
}
