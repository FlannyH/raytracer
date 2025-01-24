struct RootConstants {
    uint texture;
    uint target_width;
    uint target_height;
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

#define MASK_ID ((1 << 27) - 1)
#define PI 3.14159265358979f

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    RWTexture2D<float3> texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.texture & MASK_ID)];

    const uint2 xy = dispatch_thread_id.xy;

    if (xy.x >= root_constants.target_width) return;
    if (xy.y >= root_constants.target_height) return;
    if (texture[xy].b == 0.0) {
        float2 src_pixel = texture[xy].rg * 2.0 - 1.0;
        float reconstructed_b = sqrt(1.0 - (src_pixel.x * src_pixel.x) - (src_pixel.y * src_pixel.y));
        float3 output_pixel = normalize(float3(src_pixel, reconstructed_b));
        texture[xy] = (output_pixel + 1.0) / 2.0;
    }
}
