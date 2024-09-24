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
    ResourceHandle tex;
};

struct CameraMatricesPacket {
    float4x4 view_matrix;
    float4x4 projection_matrix;
};

struct RootConstants {
    ResourceHandle packet_buffer;
    uint camera_matrices_offset;
    uint draw_mesh_packet_offset;
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

struct VertexOut {
    float3 normal : NORMAL0;
    float4 tangent : TANGENT0;
    float4 color : COLOR0;
    float2 texcoord0 : TEXCOORD0;
};

struct PixelOut {
    float4 color : SV_Target0;
    float4 normal : SV_Target1;
    float2 metal_roughness : SV_Target2;
    float4 emissive : SV_Target3;
};

sampler tex_sampler : register(s0);

#define MASK_ID ((1 << 27) - 1)

PixelOut main(in float4 position : SV_Position, in VertexOut input)
{
    ByteAddressBuffer packet_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.packet_buffer.id)];
    DrawMeshPacket packet = packet_buffer.Load<DrawMeshPacket>(root_constants.draw_mesh_packet_offset);

    PixelOut output;
    
    // Color 
    output.color = input.color;
   
    if (packet.tex.is_loaded == 1) {
        Texture2D<float4> tex = ResourceDescriptorHeap[NonUniformResourceIndex(packet.tex.id)];
        float4 tex_color = tex.Sample(tex_sampler, input.texcoord0);
        output.color *= tex_color;
    }
    
    output.normal = float4((input.normal + 1.0) * 0.5, 1.0f);
    
    // todo: implement pbr
    output.metal_roughness = float2(0.0f, 0.0f);
    output.emissive = float4(0.0f, 0.0f, 0.0f, 0.0f);
    
    return output;
}