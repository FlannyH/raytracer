struct RootConstants {
    uint reset_acc_buffer;
    uint enable_anti_aliasing;
    uint n_samples;
    uint n_bounces;
    uint tlas;
    uint accumulation_texture;
    uint output_texture;
    uint curr_sky_cube;
    uint material_buffer;
    uint view_data_buffer;
    uint view_data_buffer_offset;
    uint frame_index;
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

struct SurfaceInfo {
    float4 color;
    float3 normal_pbr;
    float3 normal_geo;
    float roughness;
    float3 emissive;
    float metallic;
};

struct VertexCompressed {
    uint pos_x : 16;
    uint pos_y : 16;
    uint pos_z : 16;
    uint material_id : 16;
    uint normal_x : 8;
    uint normal_y : 8;
    uint normal_z : 8;
    uint flags1_tangent_sign: 1;
    uint flags1_reserved : 7;
    uint tangent_x: 8;
    uint tangent_y: 8;
    uint tangent_z: 8;
    uint flags2_reserved : 8;
    uint color_r : 16;
    uint color_g : 16;
    uint color_b : 16;
    uint color_a : 16;
    float2 texcoord0;
};

struct ResourceHandle {
    uint id: 27;
    uint is_loaded: 1;
    uint type: 4;
};

struct Material {
    float4 color_multiplier;
    float3 emissive_multiplier;
    ResourceHandle color_texture;
    ResourceHandle normal_texture;
    ResourceHandle metal_roughness_texture;
    ResourceHandle emissive_texture;
    float normal_intensity;
    float roughness_multiplier;
    float metallic_multiplier;
    uint reserved0;
    uint reserved1;
};

struct Vertex {
    float3 position;
    float3 normal;
    float3 tangent;
    float3 bitangent;
    float4 color;
    float2 texcoord0;
};

sampler tex_sampler : register(s0);
sampler tex_sampler_clamp : register(s1);
sampler cube_sampler : register(s2);

#define MASK_ID ((1 << 27) - 1)
#define MASK_IS_LOADED (1 << 27)
#define PI 3.14159265358979f
#define FULLBRIGHT_NITS 200.0f

template<typename T>
T mix(T x, T y, T a) {
    return x * (1 - a) + y * a;
}

float radical_inverse_vdc(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u); 
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
uint pcg_hash(uint input) {
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}


// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float2 hammersley(uint i, uint n) {
    return float2(float(i)/float(n), radical_inverse_vdc(i));
}

float3 cosine_weighted_sample_diffuse(float2 xi, float3 n) {
    const float phi = 2 * PI * xi.x;
    const float cos_theta = sqrt(1.0f - xi.y);
    const float sin_theta = sqrt(xi.y);

    const float3 h = float3(
        sin_theta * cos(phi),
        sin_theta * sin(phi),
        cos_theta
    );

    const float3 up = (abs(n.z) < 0.999) ? (float3(0, 0, 1)) : (float3(1, 0, 0));
    const float3 tangent_x = normalize(cross(up, n));
    const float3 tangent_y = cross(n, tangent_x);
    return (tangent_x * h.x) + (tangent_y * h.y) + (n * h.z);
}

float3 importance_sample_ggx(float2 xi, float3 r, float roughness) {
    const float a = roughness;

    const float phi = 2 * PI * xi.x;
    const float cos_theta = sqrt((1 - xi.y) / (1 + (a*a - 1) * xi.y));
    const float sin_theta = sqrt(1 - cos_theta * cos_theta);

    const float3 h = float3(
        sin_theta * cos(phi),
        sin_theta * sin(phi),
        cos_theta
    );

    const float3 up = (abs(r.z) < 0.999) ? (float3(0, 0, 1)) : (float3(1, 0, 0));
    const float3 tangent_x = normalize(cross(up, r));
    const float3 tangent_y = cross(r, tangent_x);
    return (tangent_x * h.x) + (tangent_y * h.y) + (r * h.z);
}

float3 fresnel_schlick(float cos_theta, float3 f0, float roughness) {
    float3 smooth = 1.0 - roughness;
    return f0 + (max(smooth, f0) - f0) * pow(1.0f - cos_theta, 5.0f);
}

float geometry_schlick_ggx(float n_dot_v, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num   = n_dot_v;
    float denom = n_dot_v * (1.0 - k) + k;
	
    return num / denom;
}

float3 rotate_vector_by_quaternion(float3 vec, Quaternion quat) {
    float3 quat_vec = quat.xyz;
    float quat_scalar = quat.w;
    float3 rotated_vec = 2.0f * dot(quat_vec, vec) * quat_vec
                       + (quat_scalar * quat_scalar - dot(quat_vec, quat_vec)) * vec
                       + 2.0f * quat_scalar * cross(quat_vec, vec);
    return rotated_vec;
}

SurfaceInfo get_surface_info(uint triangle_index, uint mesh_handle, float2 barycentric_coords, float3x4 obj_to_world_matrix) {
    uint vertex_index = triangle_index * 3;
    ByteAddressBuffer vertex_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(mesh_handle)];
    
    // Decompress vertices
    Vertex verts[3];
    uint material_id = 0;
    for (uint i = 0; i < 3; ++i) {
        VertexCompressed vert_compressed = vertex_buffer.Load<VertexCompressed>((vertex_index + i) * sizeof(VertexCompressed));
        verts[i].normal.x = ((float)vert_compressed.normal_x - 127.0f) / 127.0f;
        verts[i].normal.y = ((float)vert_compressed.normal_y - 127.0f) / 127.0f;
        verts[i].normal.z = ((float)vert_compressed.normal_z - 127.0f) / 127.0f;
        verts[i].tangent.x = ((float)vert_compressed.tangent_x - 127.0f) / 127.0f;
        verts[i].tangent.y = ((float)vert_compressed.tangent_y - 127.0f) / 127.0f;
        verts[i].tangent.z = ((float)vert_compressed.tangent_z - 127.0f) / 127.0f;
        float tangent_sign = ((float)vert_compressed.flags1_tangent_sign * 2.0f) - 1.0f;
        verts[i].bitangent = cross(verts[i].normal.xyz, verts[i].tangent.xyz) * tangent_sign;
        verts[i].color.r = (float) vert_compressed.color_r / 1023.0f;
        verts[i].color.g = (float) vert_compressed.color_g / 1023.0f;
        verts[i].color.b = (float) vert_compressed.color_b / 1023.0f;
        verts[i].color.a = (float) vert_compressed.color_a / 1023.0f;
        verts[i].texcoord0 = vert_compressed.texcoord0;
        if (i == 0) material_id = vert_compressed.material_id;
    }

    // Interpolate vertices
    SurfaceInfo info;
    float2 bary = barycentric_coords;
    float2 texcoord0 = verts[0].texcoord0 + ((verts[1].texcoord0 - verts[0].texcoord0) * bary.x) + ((verts[2].texcoord0 - verts[0].texcoord0) * bary.y);
    info.normal_geo  = verts[0].normal    + ((verts[1].normal    - verts[0].normal)    * bary.x) + ((verts[2].normal    - verts[0].normal)    * bary.y);
    info.color       = verts[0].color     + ((verts[1].color     - verts[0].color)     * bary.x) + ((verts[2].color     - verts[0].color)     * bary.y);
    info.metallic  = 0.0f;
    info.roughness = 1.0f;
    info.emissive  = 0.0f;
    
    // Transform to world space
    info.normal_geo = mul(obj_to_world_matrix, float4(info.normal_geo, 0)).xyz;

    // Apply material
    if (material_id >= 0) {
        ByteAddressBuffer material_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.material_buffer & MASK_ID)];
        Material material = material_buffer.Load<Material>(((uint) round(material_id)) * 64);

        // Color
        if (material.color_texture.is_loaded != 0) {
            Texture2D<float4> tex = ResourceDescriptorHeap[NonUniformResourceIndex(material.color_texture.id)];
            float4 tex_color = tex.Sample(tex_sampler, texcoord0);
            info.color *= tex_color;
        }

        // Normal
        if (material.normal_texture.is_loaded != 0) {
            float3 bitangent = verts[0].bitangent + ((verts[1].bitangent - verts[0].bitangent) * bary.x) + ((verts[2].bitangent - verts[0].bitangent) * bary.y);
            float3 tangent   = verts[0].tangent   + ((verts[1].tangent   - verts[0].tangent)   * bary.x) + ((verts[2].tangent   - verts[0].tangent)   * bary.y);
            tangent   = mul(obj_to_world_matrix, float4(tangent,   0)).xyz;
            bitangent = mul(obj_to_world_matrix, float4(bitangent, 0)).xyz;

            Texture2D<float3> tex = ResourceDescriptorHeap[NonUniformResourceIndex(material.normal_texture.id)];
            float3 tex_normal = (tex.Sample(tex_sampler, texcoord0) * 2.0f) - 1.0f;
            float3 default_normal = float3(0.0f, 0.0f, 1.0f);
            float3 interpolated_normal = lerp(default_normal, tex_normal, material.normal_intensity);
            float3x3 tbn = transpose(float3x3(tangent, bitangent, info.normal_geo));
            info.normal_pbr = mul(tbn, interpolated_normal);
        }
        else {
            info.normal_pbr = info.normal_geo;
        }
        info.normal_geo = normalize(info.normal_geo);
        info.normal_pbr = normalize(info.normal_pbr);

        // Metal & roughness
        info.metallic  = material.metallic_multiplier;
        info.roughness = material.roughness_multiplier;
        
        if (material.metal_roughness_texture.is_loaded != 0) {
            Texture2D<float4> tex = ResourceDescriptorHeap[NonUniformResourceIndex(material.metal_roughness_texture.id)];
            float2 metal_roughness = tex.Sample(tex_sampler, texcoord0).zy;
            info.metallic  *= metal_roughness.x;
            info.roughness *= metal_roughness.y;
        }

        // Emissive
        if (material.emissive_texture.is_loaded != 0) {
            Texture2D<float4> tex = ResourceDescriptorHeap[NonUniformResourceIndex(material.emissive_texture.id)];
            float3 tex_emissive = tex.Sample(tex_sampler, texcoord0).xyz;
            info.emissive = tex_emissive * material.emissive_multiplier;
        }
    }

    return info;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    // Generate random number for this pixel
    uint sample_index = 0;
    const uint n_accum_frames = 10240;
    uint n_sample_indices = 67 * 67 * root_constants.n_bounces * root_constants.n_samples * n_accum_frames;
    sample_index = (sample_index * 0)                        + (dispatch_thread_id.x % 67);
    sample_index = (sample_index * 67)                       + (dispatch_thread_id.y % 67);
    sample_index = (sample_index * n_accum_frames)           + (root_constants.frame_index);
    sample_index = pcg_hash(sample_index);

    RWTexture2D<float3> output_texture       = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.output_texture       & MASK_ID)];
    RWTexture2D<float4> accumulation_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.accumulation_texture & MASK_ID)];

    if (root_constants.reset_acc_buffer) {
        accumulation_texture[dispatch_thread_id.xy] = float4(0.0, 0.0, 0.0, 0.0);
    }

    // Get normalized screen UV coordinates, from -1.0 to +1.0
    float2 resolution;
    output_texture.GetDimensions(resolution.x, resolution.y);
    float2 jitter = 0.0;
    if (root_constants.enable_anti_aliasing) {
        jitter = float2(
            float((sample_index >> 0) % 256) / 256.0,
            float((sample_index >> 8) % 256) / 256.0
        );
    }
    float2 uv = ((float2(dispatch_thread_id.xy) + jitter) / resolution) * 2.0 - 1.0;
    
    // Calculate view direction
    ByteAddressBuffer view_data_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.view_data_buffer & MASK_ID)];
    ViewData view_data = view_data_buffer.Load<ViewData>(root_constants.view_data_buffer_offset);
    const float3 view_direction_vs = normalize(float3(view_data.viewport_size * float2(uv.x, -uv.y), -1.0f));
    const float3 view_direction_ws = normalize(rotate_vector_by_quaternion(view_direction_vs, view_data.forward));

    TextureCube<float4> sky_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.curr_sky_cube & MASK_ID)];
    RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[root_constants.tlas & MASK_ID];
    RayDesc ray;
    ray.TMin = 0.000;
    ray.TMax = 100000.0;
    float3 light = 0.0;
    bool primary_hit_sky = false;

    for (uint s = 0; s < root_constants.n_samples; ++s) {
        float3 ray_tint = 1.0;
        ray.Origin = view_data.camera_world_position;
        ray.Direction = view_direction_ws;
        
        for (uint i = 0; i < root_constants.n_bounces; ++i) {
            ray.TMin = 0.000;
            ray.TMax = 100000.0;

            // Get new random number
            sample_index = (sample_index * root_constants.n_bounces) + (i);
            sample_index = (sample_index * root_constants.n_samples) + (s);
            sample_index = pcg_hash(sample_index);

            // Trace
            RayQuery<RAY_FLAG_FORCE_OPAQUE> ray_query;
            ray_query.TraceRayInline(tlas, 0, 0xFF, ray);
            while(ray_query.Proceed());

            // Miss? Sample sky
            if (ray_query.CommittedStatus() == COMMITTED_NOTHING) {
                float3 sky = sky_texture.SampleLevel(cube_sampler, normalize(ray.Direction), 0).rgb;
                float multiplier = (i == 0) ? (0.5f) : (1.0f);
                light += sky * ray_tint * multiplier;
                break;
            }

            // Get vertex attributes
            uint triangle_index = ray_query.CandidatePrimitiveIndex();
            uint vertex_buffer_handle = ray_query.CommittedInstanceID();
            SurfaceInfo info = get_surface_info(triangle_index, vertex_buffer_handle, ray_query.CandidateTriangleBarycentrics(), ray_query.CommittedObjectToWorld3x4());

            // output_texture[dispatch_thread_id.xy].xyz = info.normal_pbr;
            // return;

            // Add contribution and pick a random direction along the normal for the next ray
            light += info.emissive * ray_tint * saturate(dot(info.normal_pbr, -ray.Direction));
            
            // todo: transparency
            ray.Origin += ray_query.CommittedRayT() * ray.Direction;

            float3 metal3 = info.metallic;
            float3 f0_dielectric = 0.04f;
            float3 f0 = mix(f0_dielectric, info.color.rgb, metal3);

            uint jittered_checkerboard = ((dispatch_thread_id.x % 2) ^ (dispatch_thread_id.y % 2) ^ (root_constants.frame_index % 2));
            float random_float = float(sample_index % 65536) / 65536.0;

            if (jittered_checkerboard && random_float > info.metallic) {
                // Diffuse
                ray.Direction = cosine_weighted_sample_diffuse(hammersley(sample_index % n_sample_indices, n_sample_indices), info.normal_pbr);
                ray.Origin += info.normal_geo * 0.0001; // Bias against self intersection
                ray_tint *= info.color.rgb;
            }
            else {
                // Specular
                float3 reflection = reflect(ray.Direction, info.normal_pbr);
                float2 xi = hammersley(sample_index % n_sample_indices, n_sample_indices);

                // Low roughness values cause float precision issues, which results in NaNs
                float roughness2 = clamp(info.roughness * info.roughness, 0.01, 1.0); 
                ray.Direction = importance_sample_ggx(xi, reflection, roughness2);
                ray.Origin += ray.Direction * 0.0001; // Bias against self intersection

                // Fresnel
                float n_dot_d = saturate(dot(ray.Direction, info.normal_pbr));
                float3 specular_f = fresnel_schlick(n_dot_d, f0, roughness2);

                // Geometry
                float g = geometry_schlick_ggx(n_dot_d, roughness2);

                ray_tint *= specular_f * g;
            }
        }
    }

    // * 2 because we split the diffuse and specular terms
    accumulation_texture[dispatch_thread_id.xy].xyz += 2 * light / root_constants.n_samples;
    accumulation_texture[dispatch_thread_id.xy].w += 1;

    output_texture[dispatch_thread_id.xy] = FULLBRIGHT_NITS * accumulation_texture[dispatch_thread_id.xy].xyz / accumulation_texture[dispatch_thread_id.xy].w;
}