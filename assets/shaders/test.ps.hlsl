struct RootConstants
{
    uint bindings_id;
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

float4 main(in float4 position : SV_Position, in float3 color : COLOR0) : SV_Target0
{
    return float4(color, 1.0);
}