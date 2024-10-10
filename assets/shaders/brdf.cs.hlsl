struct RootConstants {
    uint output_texture;
    uint color_texture;
    uint normal_texture;
    uint metal_roughness_texture;
    uint emissive_texture;
    uint lights_buffer;
    uint curr_sky_cube;
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
#define MASK_IS_LOADED (1 << 27)
#define PI 3.14159265358979f
#define FULLBRIGHT_NITS 200.0f

float3 rotate_vector_by_quaternion(float3 vec, float4 quat) {
    float3 quat_vec = quat.xyz;
    float quat_scalar = quat.w;
    float3 rotated_vec = 2.0f * dot(quat_vec, vec) * quat_vec
                       + (quat_scalar * quat_scalar - dot(quat_vec, quat_vec)) * vec
                       + 2.0f * quat_scalar * cross(quat_vec, vec);
    return rotated_vec;
}

sampler tex_sampler : register(s0);

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    float3 out_value = float3(0.0, 0.0, 0.0);
    
    // Fetch textures
    RWTexture2D<float3> output_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.output_texture & MASK_ID)];
    Texture2D<float4> color_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.color_texture & MASK_ID)];
    Texture2D<float4> normal_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.normal_texture & MASK_ID)];
    Texture2D<float3> emissive_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.emissive_texture & MASK_ID)];
    float4 color = color_texture[dispatch_thread_id.xy];
    float4 normal = normal_texture[dispatch_thread_id.xy];
    
    // Normal buffer alpha channel is 0 if nothing was rendered, in which case:
    // - normals buffer: forward direction (quaternion)
    // - color buffer: {viewport_width, viewport_height, viewport_depth}
    // which will facilitate skybox rendering. The alpha channel would otherwise be unused, so might as well.
    if (color.a < -0.0001f) {
        if ((root_constants.curr_sky_cube && MASK_IS_LOADED) == false) {
            output_texture[dispatch_thread_id.xy].rgb = 0.0f;
            return;
        }
        // Get normalized screen UV coordinates, from -1.0 to +1.0
        float2 resolution;
        color_texture.GetDimensions(resolution.x, resolution.y);
        float2 uv = ((float2(dispatch_thread_id.xy) + 0.5) / resolution) * 2.0 - 1.0;
        
        // Calculate view direction
        float3 view_direction = normalize(color.xyz * float3(uv.x, -uv.y, 1.0));
        view_direction = rotate_vector_by_quaternion(view_direction, normal.xyzw);
        
        // Fetch texture and output
        TextureCube<float3> sky_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.curr_sky_cube & MASK_ID)];
        output_texture[dispatch_thread_id.xy].rgb = sky_texture.Sample(tex_sampler, view_direction) * FULLBRIGHT_NITS;
        
        return;
    }
    
    float3 emission = emissive_texture[dispatch_thread_id.xy];
    
    // Get light buffer
    ByteAddressBuffer packet_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.lights_buffer & MASK_ID)];
    LightCounts light_counts = packet_buffer.Load<LightCounts>(0);
    
    // Process all directional lights
    for (int i = 0; i < light_counts.n_directional_lights; i++) {
        LightDirectional light = packet_buffer.Load<LightDirectional>(12 + (28 * i));
        float n_dot_l = saturate(dot(normal.xyz, -light.direction));
        out_value += color.xyz * n_dot_l * light.intensity;
    }
    
    // Add emission - The glTF spec has this to say about emissive textures:
    // "Many rendering engines simplify this calculation by assuming that an emissive factor of 1.0 results in a fully exposed pixel."
    out_value += emission * FULLBRIGHT_NITS;
    
    output_texture[dispatch_thread_id.xy].rgb = out_value; // let's define 1.0 as 200 nits, and then apply a quick hacky sRGB gamma correction

}