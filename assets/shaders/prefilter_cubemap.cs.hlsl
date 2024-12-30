struct RootConstants {
    uint base_cubemap;
    uint target_cubemap;
    uint base_resolution;
    uint target_resolution;
    uint roughness; // fixed point value between 0 (0.0) and 65536 (1.0)
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

#define MASK_ID ((1 << 27) - 1)
#define PI 3.14159265358979f

// https://ttwong12.github.io/papers/udpoint/udpoint.pdf - based on Source Code 1
float2 hammersley(int k, float n) {
    float u = 0.0f;
    float p = 0.5f;
    int kk = k;
    while (kk > 0) {
        if ((kk & 1) == 1) {
            u += p;
        }
        p *= 0.5f;
        kk >>= 1;
    }
    float v = (k + 0.5f) / n;
    return float2(u, v);
}

// https://de45xmedrsdbp.cloudfront.net/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
float3 importance_sample_ggx(float2 xi, float roughness2, float3 n) {
    const float a = roughness2;

    const float phi = 2 * PI * xi.x;
    const float cos_theta = sqrt((1 - xi.y) / (1 + (a*a - 1) * xi.y));
    const float sin_theta = sqrt(1 - cos_theta * cos_theta);

    const float3 h = float3(
        sin_theta * cos(phi),
        sin_theta * sin(phi),
        cos_theta
    );

    const float3 up = (abs(n.z) < 0.999) ? (float3(0, 0, 1)) : (float3(1, 0, 0));
    const float3 tangent_x = normalize(cross(up, n));
    const float3 tangent_y = cross(n, tangent_x);
    return (tangent_x * h.x) + (tangent_y * h.y) + (n * h.z);
}

sampler cube_sampler : register(s1);

// https://de45xmedrsdbp.cloudfront.net/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    TextureCube<float3> base_cube_map = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.base_cubemap & MASK_ID)];
    RWTexture2DArray<float3> target_cube_map = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.target_cubemap & MASK_ID)];

    float cubemap_w = (float)root_constants.target_resolution;
    float cubemap_h = (float)root_constants.target_resolution;

    uint x = dispatch_thread_id.x;
    uint y = dispatch_thread_id.y;
    uint face = dispatch_thread_id.z;

    if (x >= cubemap_w) return;
    if (y >= cubemap_h) return;

    // Get normal vector
    float cu = ((float(x) + 0.5f) / float(cubemap_w)) * 2.0f - 1.0f;
    float cv = ((float(y) + 0.5f) / float(cubemap_h)) * 2.0f - 1.0f;
    float3 n = float3(0.0f, 1.0f, 0.0f);
    switch (face) {
    case 0: n = normalize(float3(1.0f, -cv, -cu));  break;
    case 1: n = normalize(float3(-1.0f, -cv, cu));  break;
    case 2: n = normalize(float3(cu, 1.0f, cv));    break;
    case 3: n = normalize(float3(cu, -1.0f, -cv));  break;
    case 4: n = normalize(float3(cu, -cv, 1.0f));   break;
    case 5: n = normalize(float3(-cu, -cv, -1.0f)); break;
    }
    float3 v = n;

    const float roughness = float(root_constants.roughness) / 65536.0f;
    const float roughness2 = roughness * roughness;
    const int n_samples = max(1, int(1024.0f * (pow(roughness, 1.5f) / 2.0f + 0.5f) * (512.0f / cubemap_w)));

    float max_hdr_brightness = 64.f * (1.0f - pow(roughness, 1.0f / 3.0f));
    float total_weight = 0.0f;
    float3 color = float3(0, 0, 0);
    for (int i = 0; i < n_samples; ++i) {
        const float2 xi = hammersley(i, n_samples);
        const float3 h = importance_sample_ggx(xi, roughness2, n);
        const float3 l = 2 * dot(v, h) * h - v;

        const float n_dot_l = saturate(dot(n, l));
        if (n_dot_l > 0.0f) {
            // Brightness is limited to avoid fireflies in the output image
            // todo: https://learnopengl.com/PBR/IBL/Specular-IBL - whatever Chetan Jags is doing to reduce the fireflies
            color += min(max_hdr_brightness, base_cube_map.SampleLevel(cube_sampler, l, 0) * n_dot_l);
            total_weight += n_dot_l;   
        }
    }

    target_cube_map[uint3(x, y, face)] = color / total_weight;
}
