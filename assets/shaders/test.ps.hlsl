struct ResourceHandle {
    uint id: 27;
    uint is_loaded: 1;
    uint type: 4;
};

struct DrawMeshPacket {
    float4x4 model_transform;
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
    float4 color : COLOR0;
    float2 texcoord0 : TEXCOORD0;
};

sampler tex_sampler : register(s0);

#define MASK_ID ((1 << 27) - 1)

float4 main(in float4 position : SV_Position, in VertexOut input) : SV_Target0 {
    ByteAddressBuffer packet_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.packet_buffer.id)];
    DrawMeshPacket packet = packet_buffer.Load<DrawMeshPacket>(root_constants.draw_mesh_packet_offset);

    float4 out_color = input.color;
    
    if (packet.tex.is_loaded == 1) {
        Texture2D<float4> tex = ResourceDescriptorHeap[NonUniformResourceIndex(packet.tex.id)];
        float4 tex_color = tex.Sample(tex_sampler, input.texcoord0);
        out_color *= tex_color;
    }
    
    return out_color;
}