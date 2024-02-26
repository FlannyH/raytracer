struct Vertex {
    float3 position;
    float3 normal;
    float4 tangent;
    float4 color;
    float2 texcoord0;
};

struct Bindings {
    uint vertex_buffer;
    uint tex;
};

struct RootConstants
{
    uint bindings_id;
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

struct VertexOut {
    float4 color : COLOR0;
    float2 texcoord0 : TEXCOORD0;
};

float4 main(in uint vertex_index : SV_VertexID, out VertexOut output) : SV_POSITION{
    ByteAddressBuffer bindings_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.bindings_id)];
    Bindings bindings = bindings_buffer.Load<Bindings>(0);

    ByteAddressBuffer vertex_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(bindings.vertex_buffer)];
    Vertex vert = vertex_buffer.Load<Vertex>(vertex_index * sizeof(Vertex));

    output.color = vert.color;
    output.texcoord0 = vert.texcoord0;
    return float4(vert.position, 1);
}