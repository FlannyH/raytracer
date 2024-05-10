struct Vertex {
    float3 position;
    float3 normal;
    float4 tangent;
    float4 color;
    float2 texcoord0;
};

struct DrawPacket {
    float3x4 model_transform;
    uint vertex_buffer;
    uint tex;
};

struct RootConstants
{
    uint packet_buffer;
    uint offset;
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

struct VertexOut {
    float4 color : COLOR0;
    float2 texcoord0 : TEXCOORD0;
};

#define MASK_ID ((1 << 27) - 1)

float4 main(in uint vertex_index : SV_VertexID, out VertexOut output) : SV_POSITION {
    ByteAddressBuffer packet_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.packet_buffer & MASK_ID)];
    DrawPacket packet = packet_buffer.Load<DrawPacket>(root_constants.offset);

    ByteAddressBuffer vertex_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(packet.vertex_buffer & MASK_ID)];
    Vertex vert = vertex_buffer.Load<Vertex>(vertex_index * sizeof(Vertex));

    output.color = vert.color;
    output.texcoord0 = vert.texcoord0;
    return float4(vert.position, 1);
}