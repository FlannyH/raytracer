struct ResourceHandle {
    uint id: 27;
    uint is_loaded: 1;
    uint type: 4;
};

struct DrawMeshPacket {
    float4x4 model_transform;
    float4 position_offset;
    float4 position_scale;
    ResourceHandle vertex_buffer;
};

struct CameraMatricesPacket {
    float4x4 view_matrix;
    float4x4 projection_matrix;
};

struct RootConstants {
    uint packet_buffer;
    uint camera_matrices_offset;
    uint draw_mesh_packet_offset;
    uint material_buffer;
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

struct VertexOut {
    float3 normal : NORMAL0;
    float3 tangent : TANGENT0;
    float3 bitangent : TANGENT1;
    float4 color : COLOR0;
    float3 texcoord0_materialid : TEXCOORD0;
};

struct PixelOut {
    float4 color : SV_Target0;
    float4 normal : SV_Target1;
    float2 metal_roughness : SV_Target2;
    float4 emissive : SV_Target3;
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

PixelOut main(in float4 position : SV_Position, in VertexOut input) {
    ByteAddressBuffer packet_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.packet_buffer & MASK_ID)];
    DrawMeshPacket packet = packet_buffer.Load<DrawMeshPacket>(root_constants.draw_mesh_packet_offset);

    PixelOut output;
    
    // Fill default values
    output.color = input.color;
    output.normal = float4((input.normal + 1.0) * 0.5, 1.0f);
    output.metal_roughness = float2(0.0f, 0.0f);
    output.emissive = float4(0.0f, 0.0f, 0.0f, 0.0f);
    
    // Apply material
    if (abs(input.texcoord0_materialid.z - 65535.0f) > 0.1f) {
        ByteAddressBuffer material_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.material_buffer & MASK_ID)];
        Material material = material_buffer.Load<Material>(((uint) input.texcoord0_materialid.z) * 64);

        // Color
        if (material.color_texture.is_loaded != 0) {
            Texture2D<float4> tex = ResourceDescriptorHeap[NonUniformResourceIndex(material.color_texture.id)];
            float4 tex_color = tex.Sample(tex_sampler, input.texcoord0_materialid.xy);
            output.color *= tex_color;
        }
        output.color *= material.color_multiplier;

        // Normal
        if (material.normal_texture.is_loaded != 0) {
            Texture2D<float4> tex = ResourceDescriptorHeap[NonUniformResourceIndex(material.normal_texture.id)];
            float3 tex_normal = (tex.Sample(tex_sampler, input.texcoord0_materialid.xy).xyz - 1.0f) * 2.0f;
            float3 default_normal = float3(0.0f, 0.0f, 1.0f);
            float3 interpolated_normal = lerp(default_normal, tex_normal, material.normal_intensity);
            float3 normal = 
                (tex_normal.x * input.tangent) +
                (tex_normal.y * input.bitangent) +
                (tex_normal.z * input.normal);
           
            output.normal = float4((normal + 1.0f) * 0.5f, 1.0f);
        }
    }
    
    return output;
}