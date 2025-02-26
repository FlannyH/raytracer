struct RootConstants {
    uint output_texture;
    uint position_texture;
    uint color_texture;
    uint normal_texture;
    uint metal_roughness_texture;
    uint emissive_texture;
    uint ssao_texture;
    uint lights_buffer;
    uint spherical_harmonics_buffer;
    uint curr_sky_cube;
    uint curr_specular_ibl;
    uint curr_ibl_diffuse_sh_offset;
    uint curr_specular_ibl_n_mips;
    uint view_data_buffer;
    uint view_data_buffer_offset;
    uint env_brdf_lut;
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

struct Quaternion {
    float3 xyz;
    float w;
};

struct ViewData {
    Quaternion forward;
    float2 viewport_size;
    float3 camera_world_position;
};

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

struct SphericalHarmonicsMatrices {
    float4x4 r;
    float4x4 g;
    float4x4 b;
};

#define MASK_ID ((1 << 27) - 1)
#define MASK_IS_LOADED (1 << 27)
#define PI 3.14159265358979f
#define FULLBRIGHT_NITS 200.0f

float3 rotate_vector_by_quaternion(float3 vec, Quaternion quat) {
    float3 quat_vec = quat.xyz;
    float quat_scalar = quat.w;
    float3 rotated_vec = 2.0f * dot(quat_vec, vec) * quat_vec
                       + (quat_scalar * quat_scalar - dot(quat_vec, quat_vec)) * vec
                       + 2.0f * quat_scalar * cross(quat_vec, vec);
    return rotated_vec;
}

// Sources used for the following functions:
// - https://de45xmedrsdbp.cloudfront.net/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
// - https://learnopengl.com/PBR/Lighting (which has GLSL implementations of the above)
float distribution_ggx(float n_dot_h, float roughness) {
    float a = roughness*roughness;
    float a2 = a*a;
    float n_dot_h2 = n_dot_h * n_dot_h;
	
    float num = a2;
    float denom = (n_dot_h2 * (a2 - 1.0f) + 1.0f);
    denom = PI * denom * denom;
	
    return num / denom;
}

float geometry_schlick_ggx(float n_dot_v, float roughness) {
    float r = roughness;
    float k = (r * r) / 2.0;

    float num   = n_dot_v;
    float denom = n_dot_v * (1.0 - k) + k;
	
    return num / denom;
}

float geometry_smith(float n_dot_v, float n_dot_l, float roughness) {
    float ggx2 = geometry_schlick_ggx(n_dot_v, roughness);
    float ggx1 = geometry_schlick_ggx(n_dot_l, roughness);
    return ggx1 * ggx2;
}

float3 fresnel_schlick(float v_dot_h, float3 f0, float roughness) {
    float smooth = 1.0 - roughness;
    float3 smooth3 = float3(smooth, smooth, smooth);
    return f0 + (max(smooth3, f0) - f0) * pow(1.0f - v_dot_h, 5.0f);
}


float3 mix(float3 x, float3 y, float a) {
    return x * (1-a) + y * a;
}

sampler tex_sampler : register(s0);
sampler tex_sampler_clamp : register(s1);

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    float3 out_value = float3(0.0, 0.0, 0.0);
    
    RWTexture2D<float4> output_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.output_texture & MASK_ID)];
    Texture2D<float4> color_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.color_texture & MASK_ID)];
    Texture2D<float4> normal_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.normal_texture & MASK_ID)]; // view space
    Texture2D<float4> metal_roughness_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.metal_roughness_texture & MASK_ID)];
    Texture2D<float3> emissive_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.emissive_texture & MASK_ID)];
    Texture2D<float> ssao_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.ssao_texture & MASK_ID)];
    Texture2D<float2> env_brdf_lut = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.env_brdf_lut & MASK_ID)];
    float4 color = color_texture[dispatch_thread_id.xy];

    // Get normalized screen UV coordinates, from -1.0 to +1.0
    float2 resolution;
    color_texture.GetDimensions(resolution.x, resolution.y);
    float2 uv = ((float2(dispatch_thread_id.xy) + 0.5) / resolution) * 2.0 - 1.0;

    // Calculate view direction
    ByteAddressBuffer view_data_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.view_data_buffer & MASK_ID)];
    ViewData view_data = view_data_buffer.Load<ViewData>(root_constants.view_data_buffer_offset);
    const float3 view_direction_vs = normalize(float3(view_data.viewport_size * float2(uv.x, -uv.y), -1.0f));
    const float3 view_direction_ws = rotate_vector_by_quaternion(view_direction_vs, view_data.forward);
    
    // Color buffer alpha channel is < 0 if nothing was rendered
    if (color.a < -0.0001f) {
        if ((root_constants.curr_sky_cube & MASK_IS_LOADED) == false) {
            output_texture[dispatch_thread_id.xy].rgb = 0.0f;
            return;
        }
        
        // Fetch texture and output
        TextureCube<float4> sky_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.curr_sky_cube & MASK_ID)];
        float3 pixel = sky_texture.SampleLevel(tex_sampler_clamp, normalize(view_direction_ws), 0).rgb * FULLBRIGHT_NITS;
        output_texture[dispatch_thread_id.xy] = float4(pixel, 1.0f);
        
        return;
    }
    
    float3 normal = normal_texture[dispatch_thread_id.xy].xyz;
    float3 emission = emissive_texture[dispatch_thread_id.xy];
    float ssao = ssao_texture[dispatch_thread_id.xy];
    float2 metal_roughness = metal_roughness_texture[dispatch_thread_id.xy].rg;
    float metallic = metal_roughness.r;
    float roughness = max(0.05f, pow(metal_roughness.g, 1.0f / 2.2f));
    
    // Get light buffer
    ByteAddressBuffer packet_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.lights_buffer & MASK_ID)];
    LightCounts light_counts = packet_buffer.Load<LightCounts>(0);
    
    // Process all directional lights
    float3 f0 = mix(0.04f, color.rgb, metallic);
    float3 reflect_dir = reflect(view_direction_vs, normal);
    float3 half_vector = normalize(-view_direction_vs + reflect_dir);
    float v_dot_h = saturate(dot(half_vector, -view_direction_vs));
    float n_dot_v = saturate(dot(normal, -view_direction_vs));
    float3 specular_f = fresnel_schlick(v_dot_h, f0, roughness);
    float3 k_s = specular_f;
    float3 diffuse_mul = 1.0f - specular_f;
    diffuse_mul *= 1.0 - metallic;

    float3 diffuse = float3(0.0f, 0.0f, 0.0f);
    float3 specular = float3(0.0f, 0.0f, 0.0f);
    
    const float3 f = specular_f;
    for (int i = 0; i < light_counts.n_directional_lights; i++) {
        LightDirectional light = packet_buffer.Load<LightDirectional>(12 + (28 * i));
        float n_dot_l = dot(normal, -light.direction);
        if (n_dot_l <= 0.0f) continue;
        
        diffuse += color.xyz * n_dot_l * light.intensity / PI;

        float remapped_roughness = (roughness + 0.1f) / 1.1f;
        const float3 h = normalize(-view_direction_vs + -light.direction);
        const float n_dot_h = saturate(dot(normal, h));
        const float d = distribution_ggx(n_dot_h, remapped_roughness);
        const float g = geometry_smith(n_dot_v, n_dot_l, (remapped_roughness + 1.0f) / 2.0f);
        specular += light.intensity * light.color * (d * g * f) / (0.0001f + (4.0f * n_dot_l * n_dot_v)) * n_dot_l;
    }
    
    // Add emission - The glTF spec has this to say about emissive textures:
    // "Many rendering engines simplify this calculation by assuming that an emissive factor of 1.0 results in a fully exposed pixel."
    out_value += emission * FULLBRIGHT_NITS;
    
    // Indirect diffuse from spherical harmonics
    if (root_constants.spherical_harmonics_buffer & MASK_IS_LOADED) {
        ByteAddressBuffer sh_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.spherical_harmonics_buffer & MASK_ID)];
        SphericalHarmonicsMatrices sh_matrices = sh_buffer.Load<SphericalHarmonicsMatrices>(root_constants.curr_ibl_diffuse_sh_offset);
        float4 n_t = float4(normalize(rotate_vector_by_quaternion(normal.xyz, view_data.forward)), 1.0);
        float e_r = max(0.0, dot(mul(sh_matrices.r, n_t), n_t));
        float e_g = max(0.0, dot(mul(sh_matrices.g, n_t), n_t));
        float e_b = max(0.0, dot(mul(sh_matrices.b, n_t), n_t));
        float3 indirect_diffuse = float3(e_r, e_g, e_b);
        diffuse += color.xyz * indirect_diffuse * FULLBRIGHT_NITS * PI;
    }

    // Indirect specular
    if (root_constants.curr_specular_ibl & MASK_IS_LOADED) {
        TextureCube<float4> ibl_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.curr_specular_ibl & MASK_ID)];
        float mip_level = pow(roughness, 2.0f) * (root_constants.curr_specular_ibl_n_mips);
        float3 env_sample = ibl_texture.SampleLevel(tex_sampler_clamp, normalize(rotate_vector_by_quaternion(reflect_dir, view_data.forward)), mip_level).rgb;
        float2 env_brdf = env_brdf_lut.Sample(tex_sampler_clamp, float2(n_dot_v, roughness));
        float3 indirect_specular = env_sample * (specular_f * env_brdf.x + env_brdf.y);
        specular += indirect_specular * FULLBRIGHT_NITS;
    }
    
    out_value += diffuse * diffuse_mul * ssao;
    out_value += (1.0 - diffuse_mul) * specular;

    output_texture[dispatch_thread_id.xy].rgb = out_value;
    output_texture[dispatch_thread_id.xy].a = 1.0f;
}
