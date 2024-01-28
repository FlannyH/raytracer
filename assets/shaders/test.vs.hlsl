struct Vertex {
    float3 position;
    float3 color;
};

struct Bindings {
    uint vertex_buffer;
};

struct RootConstants
{
    uint bindings_id;
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

float4 main(in uint vertex_index : SV_VertexID, out float3 color : COLOR0) : SV_POSITION{
    ByteAddressBuffer bindings_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.bindings_id)];
    Bindings bindings = bindings_buffer.Load<Bindings>(0);

    ByteAddressBuffer vertex_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(bindings.vertex_buffer)];
    Vertex vert = vertex_buffer.Load<Vertex>(vertex_index * sizeof(Vertex));

    color = vert.color;
    return float4(vert.position, 1);
}