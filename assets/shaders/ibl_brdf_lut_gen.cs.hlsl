struct RootConstants {
    uint ibl_brdf_lut;
    uint resolution;
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

#define MASK_ID ((1 << 27) - 1)
#define PI 3.14159265358979f

// todo: this is duplicated code, might want to put this in a separate file and figure out how to include them in multiple shaders
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

float geometry_schlick_ggx(float n_dot_v, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num   = n_dot_v;
    float denom = n_dot_v * (1.0 - k) + k;
	
    return num / denom;
}

float geometry_smith(float3 n, float3 v, float3 l, float roughness) {
    float n_dot_v = max(dot(n, v), 0.0);
    float n_dot_l = max(dot(n, l), 0.0);
    float ggx2  = geometry_schlick_ggx(n_dot_v, roughness);
    float ggx1  = geometry_schlick_ggx(n_dot_l, roughness);
	
    return ggx1 * ggx2;
}

sampler cube_sampler : register(s2);

// https://de45xmedrsdbp.cloudfront.net/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    RWTexture2D<float2> ibl_brdf_lut = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.ibl_brdf_lut & MASK_ID)];
    // todo: is this the right way around?
    const float roughness = max(0.001f, float(dispatch_thread_id.y) / root_constants.resolution); // small roughness value causes precision issues with importance sampling
    const float n_dot_v = max(1e-6, float(dispatch_thread_id.x) / root_constants.resolution); // not 0, otherwise we get div by zero
    const float3 n = float3(0.0, 0.0, 1.0);

    const float3 v = float3(sqrt(1.0f - n_dot_v * n_dot_v), 0, n_dot_v);
    float a = 0.0f;
    float b = 0.0f;

    const uint n_samples = 1024;
    for (uint i = 0; i < n_samples; i++) {
        const float2 xi = hammersley(i, n_samples);
        const float3 h = importance_sample_ggx(xi, roughness, n);
        const float3 l = normalize(2 * dot(v, h) * h - v);

        const float n_dot_l = saturate(l.z);
        const float n_dot_h = saturate(h.z);
        const float v_dot_h = saturate(dot(v, h));
        
        if (n_dot_l > 0.0f) {
            const float g = geometry_smith(n, v, l, roughness);
            float g_vis = (g * v_dot_h) / (n_dot_h * n_dot_v);
            float fc = pow(1.0f - v_dot_h, 5.0f);
            a += (1.0f - fc) * g_vis;
            b += fc * g_vis;
        }
    }

    ibl_brdf_lut[dispatch_thread_id.xy] = float2(a, b) / n_samples;
}
