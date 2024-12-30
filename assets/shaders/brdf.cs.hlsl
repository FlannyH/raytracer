struct RootConstants {
    uint output_texture;
    uint position_texture;
    uint color_texture;
    uint normal_texture;
    uint metal_roughness_texture;
    uint emissive_texture;
    uint lights_buffer;
    uint spherical_harmonics_buffer;
    uint curr_sky_cube;
    uint curr_ibl_diffuse_sh_offset;
    uint curr_sky_n_mips;
    uint view_data_buffer;
    uint view_data_buffer_offset;
    uint env_brdf_lut;
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

struct ViewData {
    float4 forward;
    float2 viewport_size;
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

float3 rotate_vector_by_quaternion(float3 vec, float4 quat) {
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
float distribution_ggx(float3 n, float3 h, float roughness) {
    float a = roughness*roughness;
    float a2 = a*a;
    float n_dot_h = max(dot(n, h), 0.0f);
    float n_dot_h2 = n_dot_h * n_dot_h;
	
    float num = a2;
    float denom = (n_dot_h2 * (a2 - 1.0f) + 1.0f);
    denom = PI * denom * denom;
	
    return num / denom;
}

float geometry_schlick_ggx(float n_dot_v, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num   = n_dot_v;
    float denom = n_dot_v * (1.0 - k) + k;
	
    return num / denom;
}

float geometry_smith(float3 n, float3 v, float3 l, float roughness) {
    float n_dot_v = max(dot(n, v), 0.0);
    float n_dot_l = max(dot(n, l), 0.0);
    float ggx2  = geometry_schlick_ggx(n_dot_v, roughness);
    float ggx1  = geometry_schlick_ggx(n_dot_l, roughness);
	
    return ggx1 * ggx2;
}

float3 fresnel_schlick(float v_dot_h, float3 f0, float roughness) {
    // return f0 + (1.0f - f0) * pow(2.0, (-5.55473 * v_dot_h - 6.98316) * v_dot_h);
    // return f0 + (1.0f - f0) * pow(1.0f - v_dot_h, 5.0f);

    // return f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(2.0, (-5.55473 * v_dot_h - 6.98316) * v_dot_h);
    float smooth = 1.0 - roughness;
    float3 smooth3 = float3(smooth, smooth, smooth);
    return f0 + (max(smooth3, f0) - f0) * pow(1.0f - v_dot_h, 5.0f);
}

float3 reflect(float3 i, float3 n) {
    return i - 2.0 * dot(n, i) * n;     
}

float3 mix(float3 x, float3 y, float a) {
    return x * (1-a) + y * a;
}

sampler tex_sampler : register(s0);
sampler tex_sampler_clamp : register(s1);
sampler cube_sampler : register(s2);

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    float3 out_value = float3(0.0, 0.0, 0.0);
    
    RWTexture2D<float4> output_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.output_texture & MASK_ID)];
    Texture2D<float4> color_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.color_texture & MASK_ID)];
    Texture2D<float4> normal_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.normal_texture & MASK_ID)];
    Texture2D<float4> metal_roughness_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.metal_roughness_texture & MASK_ID)];
    Texture2D<float3> emissive_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.emissive_texture & MASK_ID)];
    Texture2D<float2> env_brdf_lut = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.env_brdf_lut & MASK_ID)];
    float4 color = color_texture[dispatch_thread_id.xy];

    // Get normalized screen UV coordinates, from -1.0 to +1.0
    float2 resolution;
    color_texture.GetDimensions(resolution.x, resolution.y);
    float2 uv = ((float2(dispatch_thread_id.xy) + 0.5) / resolution) * 2.0 - 1.0;

    // Calculate view direction
    ByteAddressBuffer view_data_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.view_data_buffer & MASK_ID)];
    ViewData view_data = view_data_buffer.Load<ViewData>(root_constants.view_data_buffer_offset);

    float3 view_direction = normalize(float3(view_data.viewport_size * float2(uv.x, -uv.y), -1.0f));
    view_direction = rotate_vector_by_quaternion(view_direction, view_data.forward);
    
    // Color buffer alpha channel is < 0 if nothing was rendered
    if (color.a < -0.0001f) {
        if ((root_constants.curr_sky_cube & MASK_IS_LOADED) == false) {
            output_texture[dispatch_thread_id.xy].rgb = 0.0f;
            return;
        }
        
        // Fetch texture and output
        TextureCube<float4> sky_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.curr_sky_cube & MASK_ID)];
        float3 pixel = sky_texture.SampleLevel(cube_sampler, normalize(view_direction), 0).rgb * FULLBRIGHT_NITS;
        output_texture[dispatch_thread_id.xy] = float4(pixel, 1.0f);
        
        return;
    }
    
    float3 normal = normal_texture[dispatch_thread_id.xy].xyz;
    float3 emission = emissive_texture[dispatch_thread_id.xy];
    float2 metal_roughness = metal_roughness_texture[dispatch_thread_id.xy].rg;
    float metallic = metal_roughness.r;
    float roughness = pow(metal_roughness.g, 1.0f / 2.2f);
    
    // Get light buffer
    ByteAddressBuffer packet_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.lights_buffer & MASK_ID)];
    LightCounts light_counts = packet_buffer.Load<LightCounts>(0);
    
    // Process all directional lights
    float3 f0 = mix(0.04f, color.rgb, metallic);
    float3 reflect_dir = reflect(view_direction, normal);
    float3 half_vector = normalize(-view_direction + reflect_dir);
    float v_dot_h = saturate(dot(half_vector, -view_direction));
    float n_dot_v = saturate(dot(normal, -view_direction));
    float3 specular_f = fresnel_schlick(v_dot_h, f0, roughness);
    float3 k_s = specular_f;
    float3 diffuse_mul = 1.0f - specular_f;
    diffuse_mul *= 1.0 - metallic;

    float3 diffuse = float3(0.0f, 0.0f, 0.0f);
    float3 specular = float3(0.0f, 0.0f, 0.0f);
    
    // todo: specular
    for (int i = 0; i < light_counts.n_directional_lights; i++) {
        // Diffuse
        LightDirectional light = packet_buffer.Load<LightDirectional>(12 + (28 * i));
        float n_dot_l = saturate(dot(normal, -light.direction));
        diffuse += color.xyz * n_dot_l * light.intensity;
    }
    
    // Add emission - The glTF spec has this to say about emissive textures:
    // "Many rendering engines simplify this calculation by assuming that an emissive factor of 1.0 results in a fully exposed pixel."
    out_value += emission * FULLBRIGHT_NITS;
    
    // Indirect diffuse from spherical harmonics
    if (root_constants.spherical_harmonics_buffer & MASK_IS_LOADED) {
        ByteAddressBuffer sh_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.spherical_harmonics_buffer & MASK_ID)];
        SphericalHarmonicsMatrices sh_matrices = sh_buffer.Load<SphericalHarmonicsMatrices>(root_constants.curr_ibl_diffuse_sh_offset);
        float4 n_t = float4(normalize(normal.xyz), 1.0);
        float e_r = max(0.0, dot(mul(sh_matrices.r, n_t), n_t));
        float e_g = max(0.0, dot(mul(sh_matrices.g, n_t), n_t));
        float e_b = max(0.0, dot(mul(sh_matrices.b, n_t), n_t));
        float3 indirect_diffuse = float3(e_r, e_g, e_b);
        diffuse += color.xyz * indirect_diffuse * FULLBRIGHT_NITS * PI;
    }

    // Indirect specular
    if (root_constants.curr_sky_cube & MASK_IS_LOADED) {
        TextureCube<float4> sky_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.curr_sky_cube & MASK_ID)];
        float mip_level = pow(roughness, 1.5f) * (root_constants.curr_sky_n_mips + 1);
        float3 env_sample = sky_texture.SampleLevel(cube_sampler, normalize(reflect_dir), mip_level).rgb;
        float2 env_brdf = env_brdf_lut.Sample(tex_sampler_clamp, float2(n_dot_v, roughness));
        float3 indirect_specular = env_sample * (specular_f * env_brdf.x + env_brdf.y);
        specular += specular_f * indirect_specular * FULLBRIGHT_NITS * PI;
    }
    
    out_value += diffuse * diffuse_mul;
    out_value += specular;

    output_texture[dispatch_thread_id.xy].rgb = out_value;
    output_texture[dispatch_thread_id.xy].a = 1.0f;
}
