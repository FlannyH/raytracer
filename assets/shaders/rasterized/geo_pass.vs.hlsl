struct Vertex {
    float3 position;
    float3 normal;
    float4 tangent;
    float4 color;
    float2 texcoord0;
};

struct VertexCompressed {
    uint pos_x : 16;
    uint pos_y : 16;
    uint pos_z : 16;
    uint material_id : 16;
    uint normal_x : 8;
    uint normal_y : 8;
    uint normal_z : 8;
    uint flags1_tangent_sign: 1;
    uint flags1_reserved : 7;
    uint tangent_x: 8;
    uint tangent_y: 8;
    uint tangent_z: 8;
    uint flags2_reserved : 8;
    uint color_r : 16;
    uint color_g : 16;
    uint color_b : 16;
    uint color_a : 16;
    float2 texcoord0;
};

struct ResourceHandle {
    uint id: 27;
    uint is_loaded: 1;
    uint type: 4;
};

struct DrawMeshPacket {
    float4x4 model_transform;
    float4 position_offset;
    float4 position_scale;
    ResourceHandle vertex_buffer;
};

struct CameraMatricesPacket {
    float4x4 view_matrix;
    float4x4 projection_matrix;
};

struct RootConstants {
    ResourceHandle packet_buffer;
    uint camera_matrices_offset;
    uint draw_mesh_packet_offset;
    ResourceHandle material_buffer;
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

struct VertexOut {
    float3 position : POSITION;
    float3 normal : NORMAL0;
    float3 tangent : TANGENT0;
    float3 bitangent : TANGENT1;
    float4 color : COLOR0;
    float3 texcoord0_materialid : TEXCOORD0;
};

float4 main(in uint vertex_index : SV_VertexID, out VertexOut output) : SV_POSITION {
    ByteAddressBuffer packet_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.packet_buffer.id)];
    DrawMeshPacket draw_packet = packet_buffer.Load<DrawMeshPacket>(root_constants.draw_mesh_packet_offset);
    CameraMatricesPacket camera_matrices = packet_buffer.Load<CameraMatricesPacket>(root_constants.camera_matrices_offset);

    ByteAddressBuffer vertex_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(draw_packet.vertex_buffer.id)];
    VertexCompressed vert_compressed = vertex_buffer.Load<VertexCompressed>(vertex_index * sizeof(VertexCompressed));
    // Decompress vertex
    Vertex vert;
    vert.position.x = ((float)vert_compressed.pos_x / 65535.0f) * draw_packet.position_scale.x + draw_packet.position_offset.x;
    vert.position.y = ((float)vert_compressed.pos_y / 65535.0f) * draw_packet.position_scale.y + draw_packet.position_offset.y;
    vert.position.z = ((float)vert_compressed.pos_z / 65535.0f) * draw_packet.position_scale.z + draw_packet.position_offset.z;
    vert.normal.x = ((float)vert_compressed.normal_x - 127.0f) / 127.0f;
    vert.normal.y = ((float)vert_compressed.normal_y - 127.0f) / 127.0f;
    vert.normal.z = ((float)vert_compressed.normal_z - 127.0f) / 127.0f;
    vert.tangent.x = (float)(vert_compressed.tangent_x - 127.0f) / 127.f;
    vert.tangent.y = (float)(vert_compressed.tangent_y - 127.0f) / 127.f;
    vert.tangent.z = (float)(vert_compressed.tangent_z - 127.0f) / 127.f;
    vert.tangent.w = ((float)vert_compressed.flags1_tangent_sign * 2.0f) - 1.0f;
    vert.color.r = (float) vert_compressed.color_r / 1023.0f;
    vert.color.g = (float) vert_compressed.color_g / 1023.0f;
    vert.color.b = (float) vert_compressed.color_b / 1023.0f;
    vert.color.a = (float) vert_compressed.color_a / 1023.0f;
    vert.texcoord0 = vert_compressed.texcoord0;
    
    float4 vert_pos = mul(draw_packet.model_transform, float4(vert.position, 1));
    vert_pos = mul(camera_matrices.view_matrix, vert_pos);
    output.position = vert_pos.xyz;
    vert_pos = mul(camera_matrices.projection_matrix, vert_pos);
    
    output.color = vert.color;
    output.normal = normalize(mul((float3x3)draw_packet.model_transform, vert.normal));
    output.normal = normalize(mul((float3x3)camera_matrices.view_matrix, output.normal));
    output.tangent.xyz = normalize(mul((float3x3)draw_packet.model_transform, vert.tangent.xyz));
    output.tangent.xyz = normalize(mul((float3x3)camera_matrices.view_matrix, output.tangent.xyz));
    output.bitangent = cross(output.normal.xyz, output.tangent.xyz) * vert.tangent.w;
    output.texcoord0_materialid.xy = vert.texcoord0;
    output.texcoord0_materialid.z = (float)vert_compressed.material_id;
    return vert_pos;
}