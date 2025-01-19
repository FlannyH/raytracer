struct RootConstants {
    uint tlas;
    uint output_texture;
    uint material_buffer;
    uint view_data_buffer;
    uint view_data_buffer_offset;
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

struct Vertex {
    float3 position;
    float3 normal;
    float4 tangent;
    float4 color;
    float2 texcoord0;
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

sampler tex_sampler : register(s0);
#define MASK_ID ((1 << 27) - 1)
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

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    RWTexture2D<float3> output_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.output_texture & MASK_ID)];
    RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[root_constants.tlas & MASK_ID];

    // Get normalized screen UV coordinates, from -1.0 to +1.0
    float2 resolution;
    output_texture.GetDimensions(resolution.x, resolution.y);
    float2 uv = ((float2(dispatch_thread_id.xy) + 0.5) / resolution) * 2.0 - 1.0;
    
    // Calculate view direction
    ByteAddressBuffer view_data_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.view_data_buffer & MASK_ID)];
    ViewData view_data = view_data_buffer.Load<ViewData>(root_constants.view_data_buffer_offset);
    const float3 view_direction_vs = normalize(float3(view_data.viewport_size * float2(uv.x, -uv.y), -1.0f));
    const float3 view_direction_ws = normalize(rotate_vector_by_quaternion(view_direction_vs, view_data.forward));

    // Trace ray
    RayDesc ray;
    ray.Origin = view_data.camera_world_position;
    ray.Direction = view_direction_ws;
    ray.TMin = 0.000;
    ray.TMax = 100000.0;

    RayQuery<RAY_FLAG_FORCE_OPAQUE> ray_query;
    ray_query.TraceRayInline(tlas, 0, 0xFF, ray);
    while(ray_query.Proceed());

    // Shade pixel
    if (ray_query.CommittedStatus() != COMMITTED_NOTHING) {
        uint triangle_index = ray_query.CandidatePrimitiveIndex();
        uint vertex_index = triangle_index * 3;
        uint vertex_buffer_handle = ray_query.CommittedInstanceID();
        ByteAddressBuffer vertex_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(vertex_buffer_handle)];
        
        // Decompress vertices
        Vertex verts[3];
        uint material_id = 0;
        for (uint i = 0; i < 3; ++i) {
            VertexCompressed vert_compressed = vertex_buffer.Load<VertexCompressed>((vertex_index + i) * sizeof(VertexCompressed));
            verts[i].normal.x = ((float)vert_compressed.normal_x - 127.0f) / 127.0f;
            verts[i].normal.y = ((float)vert_compressed.normal_y - 127.0f) / 127.0f;
            verts[i].normal.z = ((float)vert_compressed.normal_z - 127.0f) / 127.0f;
            verts[i].tangent.x = (float)vert_compressed.tangent_x - 127.0f;
            verts[i].tangent.y = (float)vert_compressed.tangent_y - 127.0f;
            verts[i].tangent.z = (float)vert_compressed.tangent_z - 127.0f;
            verts[i].tangent.w = ((float)vert_compressed.flags1_tangent_sign * 2.0f) - 1.0f;
            verts[i].color.r = (float) vert_compressed.color_r / 1023.0f;
            verts[i].color.g = (float) vert_compressed.color_g / 1023.0f;
            verts[i].color.b = (float) vert_compressed.color_b / 1023.0f;
            verts[i].color.a = (float) vert_compressed.color_a / 1023.0f;
            verts[i].texcoord0 = vert_compressed.texcoord0;
            if (i == 0) material_id = vert_compressed.material_id;
        }

        // Interpolate vertices
        float2 bary = ray_query.CandidateTriangleBarycentrics();
        float2 texcoord0 = verts[0].texcoord0 + ((verts[1].texcoord0 - verts[0].texcoord0) * bary.x) + ((verts[2].texcoord0 - verts[0].texcoord0) * bary.y);
        float3 normal = verts[0].normal + ((verts[1].normal - verts[0].normal) * bary.x) + ((verts[2].normal - verts[0].normal) * bary.y);
        float4 color = verts[0].color + ((verts[1].color - verts[0].color) * bary.x) + ((verts[2].color - verts[0].color) * bary.y);
        normal = mul(ray_query.CommittedObjectToWorld3x4(), float4(normal, 0)).xyz;

        // Apply material
        float3 output = 1;
        if (material_id >= 0) {
            ByteAddressBuffer material_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.material_buffer & MASK_ID)];
            Material material = material_buffer.Load<Material>(((uint) round(material_id)) * 64);

            // Color
            if (material.color_texture.is_loaded != 0) {
                Texture2D<float4> tex = ResourceDescriptorHeap[NonUniformResourceIndex(material.color_texture.id)];
                float4 tex_color = tex.Sample(tex_sampler, texcoord0);
                output *= tex_color.rgb;
            }
        }

        output_texture[dispatch_thread_id.xy] = output * FULLBRIGHT_NITS;
    }
    else {
        output_texture[dispatch_thread_id.xy] = 0;
    }
}