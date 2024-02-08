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
    float3 color : COLOR0;
    float2 texcoord0 : TEXCOORD0;
};

sampler tex_sampler : register(s0);

float4 main(in float4 position : SV_Position, in VertexOut input) : SV_Target0
{
    ByteAddressBuffer bindings_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.bindings_id)];
    Bindings bindings = bindings_buffer.Load<Bindings>(0);

    Texture2D<float4> tex = ResourceDescriptorHeap[NonUniformResourceIndex(bindings.tex)];
    float4 tex_color = tex.Sample(tex_sampler, input.texcoord0);
    return float4(input.color * tex_color.xyz, 1.0);
}