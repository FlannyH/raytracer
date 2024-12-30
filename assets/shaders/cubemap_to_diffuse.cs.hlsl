struct RootConstants {
    uint cube_map;
    uint sh_acc_buffer;
    uint resolution;
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

struct SHCoefficients {
    float3 l00;
    float3 l11;
    float3 l10;
    float3 l1_1;
    float3 l21;
    float3 l2_1;
    float3 l2_2;
    float3 l20;
    float3 l22;
};

#define MASK_ID ((1 << 27) - 1)
#define MASK_IS_LOADED (1 << 27)

sampler cube_sampler : register(s2);

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    TextureCube<float3> cube_map = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.cube_map & MASK_ID)];
    RWStructuredBuffer<SHCoefficients> sh_acc_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.sh_acc_buffer & MASK_ID)];

    float cubemap_w = (float)root_constants.resolution;
    float cubemap_h = (float)root_constants.resolution;

    uint x = dispatch_thread_id.x;
    uint y = dispatch_thread_id.y;
    uint face = dispatch_thread_id.z;

    // Get normal vector
    float u = ((float(x) + 0.5f) / float(cubemap_w)) * 2.0f - 1.0f;
    float v = ((float(y) + 0.5f) / float(cubemap_h)) * 2.0f - 1.0f;
    float3 dir = float3(0.0f, 1.0f, 0.0f);
    switch (face) {
    case 0: dir = normalize(float3(1.0f, -v, -u));  break;
    case 1: dir = normalize(float3(-1.0f, -v, u));  break;
    case 2: dir = normalize(float3(u, 1.0f, v));    break;
    case 3: dir = normalize(float3(u, -1.0f, -v));  break;
    case 4: dir = normalize(float3(u, -v, 1.0f));   break;
    case 5: dir = normalize(float3(-u, -v, -1.0f)); break;
    }

    // Compute spherical harmonics coefficients
    float3 pixel = cube_map.Sample(cube_sampler, dir);
    SHCoefficients sh;
    sh.l00  = pixel * (0.282095f);
    sh.l11  = pixel * (0.488603f * dir.x);
    sh.l10  = pixel * (0.488603f * dir.z);
    sh.l1_1 = pixel * (0.488603f * dir.y);
    sh.l21  = pixel * (1.092548f * dir.x * dir.z);
    sh.l2_1 = pixel * (1.092548f * dir.y * dir.z);
    sh.l2_2 = pixel * (1.092548f * dir.x * dir.y);
    sh.l20  = pixel * (0.315392f * (3.0f * dir.z * dir.z - 1.0f));
    sh.l22  = pixel * (0.546274f * (dir.x * dir.x - dir.y * dir.y));

    // Store coefficients for each pixel in a row, for each face vertically in the buffer
    sh_acc_buffer[face * (cubemap_w * cubemap_h) + y * (cubemap_w) + x] = sh;
}
