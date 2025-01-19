struct Bindings {
    uint tlas;
    uint output;
};

struct Payload {
    float3 normal;
    float t;
};

[shader("raygeneration")] void main() {
    RWTexture2D<float3> output_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.output_texture & MASK_ID)];
    RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[root_constants.tlas & MASK_ID];

    uint2 pixel_coords = DispatchRaysIndex().xy;
    float2 pixel_center = pixel_coords + 0.5;
    float3 pos_ws = float3(pixel_center, 0);

    Payload payload = (Payload)0;
    payload.t = -1.0;

    RayDesc ray;
    ray.Origin = pos_ws;
    ray.Direction = float3(0, 0, 1);
    ray.TMin = 0.1;
    ray.TMax = 1000.0;

    float3 T = 0.0;

    TraceRay(tlas, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray, payload);

    if (payload.t > 0.0) {
        T += abs(payload.normal);
    } else {
        T += float3(0, 1, 0);
    }

    output_texture[pixel_coords] = float4(T, 1));
}
