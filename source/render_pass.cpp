#include "render_pass.h"
#include "device.h"
#include "shader.h"

namespace gfx {
    RenderPass::RenderPass(const Device& device) {
        D3D12_ROOT_PARAMETER1 root_parameters[1] = {
            {
                .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
                .Constants = {
                    .ShaderRegister = 0,
                    .RegisterSpace = 0,
                    .Num32BitValues = 2,
                },
                .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
            },
        };

        D3D12_STATIC_SAMPLER_DESC samplers[1] = {
            {
                .Filter = D3D12_FILTER_ANISOTROPIC,
                .AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                .AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                .AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                .MipLODBias = 0.0f,
                .MaxAnisotropy = 16,
                .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
                .BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
                .MinLOD = 0.0f,
                .MaxLOD = 100000.0f,
                .ShaderRegister = 0,
                .RegisterSpace = 0,
                .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
            }
        };

        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc = {
            .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
            .Desc_1_1 = {
                .NumParameters = 1,
                .pParameters = root_parameters,
                .NumStaticSamplers = 1,
                .pStaticSamplers = samplers,
                .Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED,
            }
        };

        // Now let's create a root signature
        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        try {
            validate(D3D12SerializeVersionedRootSignature(&root_signature_desc, &signature, &error));
            validate(device.device->CreateRootSignature(0, signature->GetBufferPointer(),
                signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));
            root_signature->SetName(L"Bindless Root Signature");
        }
        catch ([[maybe_unused]] std::exception& e) {
            const auto err_str = static_cast<const char*>(error->GetBufferPointer());
            printf("[ERROR] Error creating root signature: %s\n", err_str);
            error->Release();
            error = nullptr;
        }

        if (signature) {
            signature->Release();
            signature = nullptr;
        }
    }
}
