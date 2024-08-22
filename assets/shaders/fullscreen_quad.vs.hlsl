struct VertexOut
{
    float4 position : SV_Position;
    float2 tex_coord : TEXCOORD0;
};

VertexOut main(uint vertex_id : SV_VertexID)
{
    VertexOut output;
    
    // Define the corners of the quad
    const float2 corners[4] =
    {
        float2(-1.0, 1.0), // Top-left
        float2(1.0, 1.0), // Top-right
        float2(-1.0, -1.0), // Bottom-left
        float2(1.0, -1.0) // Bottom-right
    };

    float2 texCoords[4] =
    {
        float2(0.0, 0.0), // Top-left
        float2(1.0, 0.0), // Top-right
        float2(0.0, 1.0), // Bottom-left
        float2(1.0, 1.0) // Bottom-right
    };
    
    // Return vertex
    output.position = float4(corners[vertex_id], 0.0, 1.0);
    output.tex_coord = texCoords[vertex_id];
    return output;
}