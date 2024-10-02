struct RootConstants {
    uint output_texture;
};
ConstantBuffer<RootConstants> root_constants : register(b0, space0);

#define MASK_ID ((1 << 27) - 1)
#define PI 3.14159265358979f
#define FULLBRIGHT_NITS 200.0f

float linear_to_srgb(float input) {
    if (input <= 0.0031308) {
        return input * 12.92;
    }
    else {
        return 1.055 * pow(input, 1.0 / 2.4) - 0.055;
    }
}

float3 rgb_linear_to_srgb(float3 input) {
    return float3(linear_to_srgb(input.x), linear_to_srgb(input.y), linear_to_srgb(input.z));
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    RWTexture2D<float3> output_texture = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.output_texture & MASK_ID)];
    output_texture[dispatch_thread_id.xy].rgb /= FULLBRIGHT_NITS; // map from 0.0 - FULLBRIGHT_NITS to 0.0 - 1.0
    output_texture[dispatch_thread_id.xy].rgb = rgb_linear_to_srgb(output_texture[dispatch_thread_id.xy].rgb);

}