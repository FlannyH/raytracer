float4 main(in uint vertexIndex : SV_VertexID, out float2 texcoord : TEXCOORD0) : SV_POSITION {
    switch (vertexIndex) {
    case 0:
    {
        texcoord = float2(0, 0);
        return float4(-1, -1, 0, 1);
    }
    case 1:
    {
        texcoord = float2(1, 0);
        return float4(1, -1, 0, 1);
    }
    case 2:
    {
        texcoord = float2(0, 1);
        return float4(0, 1, 0, 1);
    }
    }
    return (0.0 / 0.0).xxxx;
}