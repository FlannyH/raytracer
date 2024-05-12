struct Vertex {
    float3 position;
    float3 normal;
    float4 tangent;
    float4 color;
    float2 texcoord0;
};

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

#define MASK_ID ((1 << 27) - 1)

float4 main(in uint vertex_index : SV_VertexID, out VertexOut output) : SV_POSITION {
    ByteAddressBuffer packet_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.packet_buffer.id)];
    DrawMeshPacket draw_packet = packet_buffer.Load<DrawMeshPacket>(root_constants.draw_mesh_packet_offset);
    CameraMatricesPacket camera_matrices = packet_buffer.Load<CameraMatricesPacket>(root_constants.camera_matrices_offset);

    ByteAddressBuffer vertex_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(draw_packet.vertex_buffer.id)];
    Vertex vert = vertex_buffer.Load<Vertex>(vertex_index * sizeof(Vertex));
    
    float4 vert_pos = mul(draw_packet.model_transform, float4(vert.position, 1));
    vert_pos = mul(camera_matrices.view_matrix, vert_pos);
    vert_pos = mul(camera_matrices.projection_matrix, vert_pos);

    output.color = vert.color;
    output.texcoord0 = vert.texcoord0;
    return vert_pos;
}