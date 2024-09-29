struct VertexOut
{
    float4 position : SV_Position;
    float2 tex_coord : TEXCOORD0;
};

VertexOut main(uint vertex_id : SV_VertexID)
{
    VertexOut output;
    
    // Define the corners of the quad
    const float2 corners[3] =
    {
        float2(-1.0, 1.0), // Top-left
        float2(3.0, 1.0), // Top-right
        float2(-1.0, -3.0) // Bottom-left
    };

    float2 texCoords[3] =
    {
        float2(0.0, 0.0), // Top-left
        float2(2.0, 0.0), // Top-right
        float2(0.0, 2.0) // Bottom-left
    };
    
    // Return vertex
    output.position = float4(corners[vertex_id], 0.0, 1.0);
    output.tex_coord = texCoords[vertex_id];
    return output;
}