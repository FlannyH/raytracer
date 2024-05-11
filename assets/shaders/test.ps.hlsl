struct DrawPacket {
    float4x4 model_transform;
    uint vertex_buffer;
    uint tex;
};


struct RootConstants {
    uint packet_buffer;
    uint offset;
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

struct VertexOut {
    float4 color : COLOR0;
    float2 texcoord0 : TEXCOORD0;
};

sampler tex_sampler : register(s0);

#define MASK_ID ((1 << 27) - 1)

float4 main(in float4 position : SV_Position, in VertexOut input) : SV_Target0 {
    ByteAddressBuffer packet_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.packet_buffer & MASK_ID)];
    DrawPacket packet = packet_buffer.Load<DrawPacket>(root_constants.offset);

    Texture2D<float4> tex = ResourceDescriptorHeap[NonUniformResourceIndex(packet.tex & MASK_ID)];
    float4 tex_color = tex.Sample(tex_sampler, input.texcoord0);
    return input.color * tex_color;
}