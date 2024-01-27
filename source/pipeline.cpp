#include "pipeline.h"

#include "render_pass.h"
#include "shader.h"
#include "device.h"

namespace gfx {
    Pipeline::Pipeline(const Device& device, const RenderPass& render_pass) {
        // Compile shaders
        auto vs = Shader("assets/shaders/test.vs.hlsl", "main", ShaderType::vertex);
        auto ps = Shader("assets/shaders/test.ps.hlsl", "main", ShaderType::pixel);

        // Make sure it worked
        if (vs.shader_blob.Get() == nullptr) {
            printf("[ERROR] Failed to create render pipeline: vertex shader compilation failed");
            return;
        }
        if (ps.shader_blob.Get() == nullptr) {
            printf("[ERROR] Failed to create render pipeline: pixel shader compilation failed");
            return;
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc{
            .pRootSignature = render_pass.root_signature.Get(),
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
                .CullMode = D3D12_CULL_MODE_NONE,
                .FrontCounterClockwise = FALSE,
                .DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
                .DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
                .SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
                .DepthClipEnable = FALSE,
                .MultisampleEnable = FALSE,
                .AntialiasedLineEnable = FALSE,
                .ForcedSampleCount = 0,
                .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
            },
            .DepthStencilState = {
                .DepthEnable = FALSE,
                .StencilEnable = FALSE,
            },
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            .NumRenderTargets = 1,
            .RTVFormats = {
                DXGI_FORMAT_R8G8B8A8_UNORM,
            },
            .SampleDesc {.Count = 1},
        };

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

        validate(pipeline_state->SetName(L"Render Pipeline State"));

    }
}
