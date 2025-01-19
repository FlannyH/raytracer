struct RootConstants {
    uint tlas;
    uint output_texture;
    uint view_data_buffer;
    uint view_data_buffer_offset;
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

struct Quaternion {
    float3 xyz;
    float w;
};

struct ViewData {
    Quaternion forward;
    float2 viewport_size;
    float3 camera_world_position;
};

#define MASK_ID ((1 << 27) - 1)
#define PI 3.14159265358979f
#define FULLBRIGHT_NITS 200.0f

float3 rotate_vector_by_quaternion(float3 vec, Quaternion quat) {
    float3 quat_vec = quat.xyz;
    float quat_scalar = quat.w;
    float3 rotated_vec = 2.0f * dot(quat_vec, vec) * quat_vec
                       + (quat_scalar * quat_scalar - dot(quat_vec, quat_vec)) * vec
                       + 2.0f * quat_scalar * cross(quat_vec, vec);
    return rotated_vec;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    RWTexture2D<float3> output_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.output_texture & MASK_ID)];
    RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[root_constants.tlas & MASK_ID];

    // Get normalized screen UV coordinates, from -1.0 to +1.0
    float2 resolution;
    output_texture.GetDimensions(resolution.x, resolution.y);
    float2 uv = ((float2(dispatch_thread_id.xy) + 0.5) / resolution) * 2.0 - 1.0;
    
    // Calculate view direction
    ByteAddressBuffer view_data_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.view_data_buffer & MASK_ID)];
    ViewData view_data = view_data_buffer.Load<ViewData>(root_constants.view_data_buffer_offset);
    const float3 view_direction_vs = normalize(float3(view_data.viewport_size * float2(uv.x, -uv.y), -1.0f));
    const float3 view_direction_ws = normalize(rotate_vector_by_quaternion(view_direction_vs, view_data.forward));

    // Trace ray
    RayDesc ray;
    ray.Origin = view_data.camera_world_position;
    ray.Direction = view_direction_ws;
    ray.TMin = 0.000;
    ray.TMax = 100000.0;

    RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER> ray_query;
    ray_query.TraceRayInline(tlas, 0, 0xFF, ray);
    while(ray_query.Proceed());

    // Shade pixel
    if (ray_query.CommittedStatus() != COMMITTED_NOTHING) {
        output_texture[dispatch_thread_id.xy] = ray_query.CommittedRayT() * 50;
    }
    else {
        output_texture[dispatch_thread_id.xy] = 0;
    }
}