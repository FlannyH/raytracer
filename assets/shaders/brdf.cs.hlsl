struct RootConstants {
    uint output_texture;
    uint color_texture;
    uint normal_texture;
    uint metal_roughness_texture;
    uint emissive_texture;
    uint m_lights_buffer;
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

#define MASK_ID ((1 << 27) - 1)

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    RWTexture2D<float4> output_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.output_texture & MASK_ID)];
    Texture2D<float4> color_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.color_texture & MASK_ID)];
    Texture2D<float3> normal_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.normal_texture & MASK_ID)];
    float4 color = color_texture[dispatch_thread_id.xy];
    float4 normal = float4(normal_texture[dispatch_thread_id.xy], 1.0f);
    if (dispatch_thread_id.x > 300)
        output_texture[dispatch_thread_id.xy] = color;
    else
        output_texture[dispatch_thread_id.xy] = normal;
}