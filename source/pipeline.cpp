#include "pipeline.h"
#include "shader.h"
#include "device.h"

namespace gfx {
    Pipeline::Pipeline(const Device& device, const std::string& name, const std::string& vertex_shader_path, const std::string& pixel_shader_path, const std::vector<DXGI_FORMAT> render_target_formats, const DXGI_FORMAT depth_target_format) : name(name)  {
        assert(render_target_formats.size() <= 8 && "Too many render targets!");

        // Compile shaders
        const auto vs = Shader(vertex_shader_path, "main", ShaderType::vertex); // todo: add customizable entry point
        const auto ps = Shader(pixel_shader_path, "main", ShaderType::pixel); // todo: add customizable entry point

        // Make sure it worked
        if (vs.shader_blob.Get() == nullptr) {
            LOG(Error, "Failed to create render pipeline: vertex shader compilation failed");
            return;
        }
        if (ps.shader_blob.Get() == nullptr) {
            LOG(Error, "Failed to create render pipeline: pixel shader compilation failed");
            return;
        }

        create_global_root_signature(device);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc {
            .pRootSignature = root_signature.Get(),
            .VS = {
                vs.shader_blob->GetBufferPointer(),
                vs.shader_blob->GetBufferSize()
            },
            .PS = {
                ps.shader_blob->GetBufferPointer(),
                ps.shader_blob->GetBufferSize()
            },
            .SampleMask = UINT_MAX,
            .RasterizerState = {
                .FillMode = D3D12_FILL_MODE_SOLID,
                .CullMode = D3D12_CULL_MODE_BACK,
                .FrontCounterClockwise = TRUE,
                .DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
                .DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
                .SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
                .DepthClipEnable = TRUE,
                .MultisampleEnable = FALSE,
                .AntialiasedLineEnable = FALSE,
                .ForcedSampleCount = 0,
                .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
            },
            .DepthStencilState = {
                .DepthEnable = (depth_target_format != DXGI_FORMAT_UNKNOWN),
                .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
                .DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
                .StencilEnable = false,
            },
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            .NumRenderTargets = (UINT)render_target_formats.size(),
            .DSVFormat = depth_target_format,
            .SampleDesc {.Count = 1},
        };

        size_t i = 0;
        for (size_t i = 0; i < render_target_formats.size(); ++i) {
            pipeline_state_desc.RTVFormats[i] = render_target_formats[i];
        }

        pipeline_state_desc.BlendState.RenderTarget[0] = {
            .BlendEnable = FALSE,
            .LogicOpEnable = FALSE,
            .SrcBlend = D3D12_BLEND_ONE,
            .DestBlend = D3D12_BLEND_ZERO,
            .BlendOp = D3D12_BLEND_OP_ADD,
            .SrcBlendAlpha = D3D12_BLEND_ONE,
            .DestBlendAlpha = D3D12_BLEND_ZERO,
            .BlendOpAlpha = D3D12_BLEND_OP_ADD,
            .LogicOp = D3D12_LOGIC_OP_NOOP,
            .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
        };

        validate(device.device->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&pipeline_state)));
        validate(pipeline_state->SetName(L"Render Pipeline State")); // todo: add customizable name with default parameter
    }

    Pipeline::Pipeline(const Device& device, const std::string& name, const std::string& compute_shader_path) : name(name) {
        // Compile shader
        const auto cs = Shader(compute_shader_path, "main", ShaderType::compute); // todo: add customizable entry point

        // Make sure it worked
        if (cs.shader_blob.Get() == nullptr) {
            LOG(Error, "");
            return;
        }

        create_global_root_signature(device);

        D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline_state_desc {
            .pRootSignature = root_signature.Get(),
            .CS = {
                cs.shader_blob->GetBufferPointer(),
                cs.shader_blob->GetBufferSize()
            },
        };

        validate(device.device->CreateComputePipelineState(&pipeline_state_desc, IID_PPV_ARGS(&pipeline_state)));
        validate(pipeline_state->SetName(L"Compute Pipeline State")); // todo: add customizable name with default parameter
    }
    
    void Pipeline::create_global_root_signature(const gfx::Device &device) {
        // Create global root signature
        D3D12_ROOT_PARAMETER1 root_parameters[1] = {
            {
                .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
                .Constants = {
                    .ShaderRegister = 0,
                    .RegisterSpace = 0,
                    .Num32BitValues = 16,
                },
                .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
            },
        };

        D3D12_STATIC_SAMPLER_DESC samplers[3] = {
            { // Regular texture
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
            },
            { // Lookup texture (clamp)
                .Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
                .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                .MipLODBias = 0.0f,
                .MaxAnisotropy = 16,
                .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
                .BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
                .MinLOD = 0.0f,
                .MaxLOD = 100000.0f,
                .ShaderRegister = 1,
                .RegisterSpace = 0,
                .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
            },
            { // Cubemap
                .Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
                .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                .MipLODBias = 0.0f,
                .MaxAnisotropy = 16,
                .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
                .BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
                .MinLOD = 0.0f,
                .MaxLOD = 100000.0f,
                .ShaderRegister = 2,
                .RegisterSpace = 0,
                .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
            }
        };

        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc = {
            .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
            .Desc_1_1 = {
                .NumParameters = 1,
                .pParameters = root_parameters,
                .NumStaticSamplers = 3,
                .pStaticSamplers = samplers,
                .Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED,
            }
        };

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
            LOG(Error, "Error creating root signature: %s", err_str);
            error->Release();
            error = nullptr;
        }

        if (signature) {
            signature->Release();
            signature = nullptr;
        }
    }
}
