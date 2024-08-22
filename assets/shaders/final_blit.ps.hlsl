struct ResourceHandle
{
    uint id: 27;
    uint is_loaded: 1;
    uint type: 4;
};

struct VertexOut
{
    float4 position : SV_Position;
    float2 tex_coord : TEXCOORD0;
};

struct RootConstants
{
    ResourceHandle texture_to_blit;
};

ConstantBuffer<RootConstants> root_constants : register(b0, space0);

float4 main(in float4 position : SV_Position, in VertexOut input) : SV_Target0
{
    Texture2D<float4> tex = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.texture_to_blit.id)];
    return tex.Sample(tex_sampler, input.texcoord0);
}