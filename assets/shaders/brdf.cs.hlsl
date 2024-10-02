struct RootConstants {
    uint output_texture;
    uint color_texture;
    uint normal_texture;
    uint metal_roughness_texture;
    uint emissive_texture;
    uint m_lights_buffer;
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

struct LightCounts {
    uint n_directional_lights;
    uint n_point_lights;
    uint n_spot_lights;
};

struct LightDirectional {
    float3 color; // linear 0.0 - 1.0
    float intensity; // in lux (lm/m^2)
    float3 direction;
};

#define MASK_ID ((1 << 27) - 1)
#define PI 3.14159265358979f

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    float3 out_value = float3(0.0, 0.0, 0.0);
    
    // Fetch textures
    RWTexture2D<float3> output_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.output_texture & MASK_ID)];
    Texture2D<float3> color_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.color_texture & MASK_ID)];
    Texture2D<float3> normal_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.normal_texture & MASK_ID)];
    float3 color = color_texture[dispatch_thread_id.xy];
    float3 normal = normal_texture[dispatch_thread_id.xy];
    
    // Get light buffer
    ByteAddressBuffer packet_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.m_lights_buffer & MASK_ID)];
    LightCounts light_counts = packet_buffer.Load<LightCounts>(0);
    
    // Process all directional lights
    for (int i = 0; i < light_counts.n_directional_lights; i++) {
        LightDirectional light = packet_buffer.Load<LightDirectional>(12 + (28 * i));
        float n_dot_l = saturate(dot(normal, -light.direction));
        out_value += color * n_dot_l * light.intensity;
    }
    
    output_texture[dispatch_thread_id.xy].rgb = pow(out_value / (200.0f * PI), 1.0/2.4f); // let's define 1.0 as 200 nits, and then apply a quick hacky sRGB gamma correction

}