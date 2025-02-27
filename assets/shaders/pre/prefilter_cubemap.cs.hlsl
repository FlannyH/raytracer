struct RootConstants {
    uint base_cubemap;
    uint target_cubemap;
    uint base_resolution;
    uint target_resolution;
    uint roughness; // fixed point value between 0 (0.0) and 65536 (1.0)
    uint quality; // fixed point value between 0 (0.0) and 65536 (1.0)
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

#define MASK_ID ((1 << 27) - 1)
#define PI 3.14159265358979f

float radical_inverse_vdc(uint bits) {
     bits = (bits << 16u) | (bits >> 16u);
     bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
     bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
     bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
     bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
     return float(bits) * 2.3283064365386963e-10; // / 0x100000000
 }

 // http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
 float2 hammersley(uint i, uint n) {
     return float2(float(i)/float(n), radical_inverse_vdc(i));
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

// Sources used for the following functions:
// - https://de45xmedrsdbp.cloudfront.net/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
// - https://learnopengl.com/PBR/Lighting (which has GLSL implementations of the above)
float distribution_ggx(float n_dot_h, float roughness) {
    float a = roughness*roughness;
    float a2 = a*a;
    float n_dot_h2 = n_dot_h * n_dot_h;
	
    float num = a2;
    float denom = (n_dot_h2 * (a2 - 1.0f) + 1.0f);
    denom = PI * denom * denom;
	
    return num / denom;
}

sampler tex_sampler_clamp : register(s1);

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
    const float quality = float(root_constants.quality) / 65536.f;
    const int n_samples = max(1, int(1024.0f * quality * (pow(roughness, 2.0f) / 2.0f + 0.5f) * (512.0f / cubemap_w)));

    float max_hdr_brightness = sqrt(quality) * 1600.f * (1.0f - pow(roughness, 1.0f / 3.0f));
    float total_weight = 0.0f;
    float3 color = float3(0, 0, 0);
    for (int i = 0; i < n_samples; ++i) {
        const float2 xi = hammersley(i, n_samples);
        const float3 h = importance_sample_ggx(xi, roughness, n);
        const float h_dot_v = dot(h, v);
        const float3 l = 2 * (h_dot_v) * h - v;

        const float n_dot_l = saturate(dot(n, l));
            // Select mip level to sample
            float n_dot_h = dot(n, h);
            float d = distribution_ggx(n_dot_h, roughness);
            float pdf = (d * n_dot_h / (4.0f * h_dot_v)) + 0.0001f;
            float sa_pixel = 4.0 * PI / (6.0 * cubemap_w * cubemap_h);
            float sa_sample = 1.0 / (float(n_samples) * pdf + 0.0001);
            float mip = roughness == 0.0 ? 0.0 : 0.5 * log2(sa_sample / sa_pixel);

            color += min(max_hdr_brightness, base_cube_map.SampleLevel(tex_sampler_clamp, l, mip));
            total_weight += 1.0;   
    }

    target_cube_map[uint3(x, y, face)] = color / total_weight;
}
