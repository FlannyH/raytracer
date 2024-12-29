struct RootConstants {
    uint coeff_buffer;
    uint sh_matrices_buffer;
    uint sh_matrices_buffer_byte_offset;
    uint n_elements_in_original_buffer;
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
#define sizeof_shcoefficients (9 * 12)

#define MASK_ID ((1 << 27) - 1)

[numthreads(1, 1, 1)]
void main(uint3 thread_id : SV_DispatchThreadID) {
    ByteAddressBuffer coeff_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.coeff_buffer & MASK_ID)];
    RWByteAddressBuffer sh_matrices_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(root_constants.sh_matrices_buffer & MASK_ID)];

    if (thread_id.x == 0) {
        // Load coefficients
        SHCoefficients sh = coeff_buffer.Load<SHCoefficients>(0);
        const float n = float(root_constants.n_elements_in_original_buffer);
        const float3 l00  =  sh.l00 / n;
        const float3 l11  =  sh.l11 / n;
        const float3 l10  =  sh.l10 / n;
        const float3 l1_1 = sh.l1_1 / n;
        const float3 l21  =  sh.l21 / n;
        const float3 l2_1 = sh.l2_1 / n;
        const float3 l2_2 = sh.l2_2 / n;
        const float3 l20  =  sh.l20 / n;
        const float3 l22  =  sh.l22 / n;

        // Construct matrix
        const float c1 = 0.429043f;
        const float c2 = 0.511664f;
        const float c3 = 0.743125f;
        const float c4 = 0.886227f;
        const float c5 = 0.247708f;

        // todo: test if doing this on 3 threads is faster
        float4x4 mat_r = float4x4(
            c1*l22.r,   c1*l2_2.r, c1*l21.r,  c2*l11.r,
            c1*l2_2.r, -c1*l22.r,  c1*l2_1.r, c2*l1_1.r,
            c1*l21.r,   c1*l2_1.r, c3*l20.r,  c2*l10.r,
            c2*l11.r,   c2*l1_1.r, c2*l10.r,  c4*l00.r - c5*l20.r
        );
        float4x4 mat_g = float4x4(
            c1*l22.g,   c1*l2_2.g, c1*l21.g,  c2*l11.g,
            c1*l2_2.g, -c1*l22.g,  c1*l2_1.g, c2*l1_1.g,
            c1*l21.g,   c1*l2_1.g, c3*l20.g,  c2*l10.g,
            c2*l11.g,   c2*l1_1.g, c2*l10.g,  c4*l00.g - c5*l20.g
        );
        float4x4 mat_b = float4x4(
            c1*l22.b,   c1*l2_2.b, c1*l21.b,  c2*l11.b,
            c1*l2_2.b, -c1*l22.b,  c1*l2_1.b, c2*l1_1.b,
            c1*l21.b,   c1*l2_1.b, c3*l20.b,  c2*l10.b,
            c2*l11.b,   c2*l1_1.b, c2*l10.b,  c4*l00.b - c5*l20.b
        );

        sh_matrices_buffer.Store(root_constants.sh_matrices_buffer_byte_offset + 0, mat_r);
        sh_matrices_buffer.Store(root_constants.sh_matrices_buffer_byte_offset + 64, mat_g);
        sh_matrices_buffer.Store(root_constants.sh_matrices_buffer_byte_offset + 128, mat_b);
    }
}
