struct RootConstants {
    uint n_samples;
    uint radius; // fixed point 65536 = 1.0
    uint bias; // fixed point 65536 = 1.0
    uint strength; // fixed point 65536 = 1.0
    uint frame_index;
    uint position_texture;
    uint normal_texture;
    uint output_texture;
    uint packet_buffer;
    uint camera_matrices_offset;
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

struct CameraMatricesPacket {
    float4x4 view_matrix;
    float4x4 projection_matrix;
};

#define MASK_ID ((1 << 27) - 1)
#define PI 3.14159265358979f

sampler tex_sampler_clamp : register(s1);

float radical_inverse_vdc(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u); 
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

int hash(int n) { 
    n=(n << 13) ^ n; 
    return n * (n*n*15731 + 789221) + 1376312589; 
}

// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float2 hammersley(uint i, uint n) {
    return float2(float(i)/float(n), radical_inverse_vdc(i));
}

float3 importance_sample_ggx(float2 xi, float3 n) {
    const float phi = 2 * PI * xi.x;
    const float cos_theta = sqrt((1 - xi.y) / (1 + xi.y));
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

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    Texture2D<float3> position_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.position_texture & MASK_ID)];
    Texture2D<float3> normal_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.normal_texture & MASK_ID)];
    RWTexture2D<float3> output_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.output_texture & MASK_ID)];
    ByteAddressBuffer packet_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.packet_buffer & MASK_ID)];
    CameraMatricesPacket camera_matrices = packet_buffer.Load<CameraMatricesPacket>(root_constants.camera_matrices_offset);

    // Get parameters
    const uint n_samples = root_constants.n_samples;
    const float radius = float(root_constants.radius) / 65536.0f;
    const float bias = float(root_constants.bias) / 65536.0f;
    const float strength = float(root_constants.strength) / 65536.0f;

    // Get position and normal of current pixel (worldspace)
    const int frame_index = root_constants.frame_index;
    const float3 pos = position_texture[dispatch_thread_id.xy];
    const float3 normal = normal_texture[dispatch_thread_id.xy];

    // Collect samples
    float3 occlusion = 0.0f;
    float weight = 0.0f;
    for (uint i = 0; i < n_samples; ++i) {
        // Pick random vector on hemisphere around the normal vector (using the most scuffed way to generate a random sample index)
        uint sample_index = ((((dispatch_thread_id.x % 15731) * 15731) + (dispatch_thread_id.y % 789221)) * 789221 + i * 1376312589) + frame_index;
        const float scale = ((float(hash(sample_index) % 65536) / 65536.f) + 0.25f) / 1.25f;
        const float3 hemisphere_sample_vec = importance_sample_ggx(hammersley(hash(sample_index) % 60000, 60000), normal) * scale * radius;

        // Offset the position of the current pixel by that vector
        const float3 view_sample_pos = pos + hemisphere_sample_vec;

        // Figure out where that is in screen space
        const float4 clip_sample_pos = mul(camera_matrices.projection_matrix, float4(view_sample_pos, 1.0));
        const float2 ndc_sample_pos = clip_sample_pos.xy / clip_sample_pos.w;
        float2 screen_sample_pos = 0.5 * (ndc_sample_pos + 1.0f);
        screen_sample_pos.y = 1.0 - screen_sample_pos.y; // otherwise the sample position will be upside down for some reason

        // Get a depth sample corresponding to that position
        const float sample_depth = position_texture.Sample(tex_sampler_clamp, screen_sample_pos).z;

        // If sample depth > current pixel depth, mark that sample as occluded
        const float is_occluded = (sample_depth >= pos.z + bias) ? 1.0 : 0.0;

        // If the difference between the 2 depths is too big, ignore that sample
        const float is_within_radius = smoothstep(0.0, 1.0, radius / abs(pos.z - sample_depth));
        
        // Accumulate the result
        occlusion += is_occluded * is_within_radius;
        weight += is_within_radius;
    }
    output_texture[dispatch_thread_id.xy].rgb = pow((1.0 - (occlusion / weight)), strength);
}