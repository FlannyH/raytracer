float4 main(in float4 position : SV_Position, in float2 texcoord : TEXCOORD0) : SV_Target0
{
    return float4(texcoord.xy, 0.0, 1.0);
}