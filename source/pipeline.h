#pragma once
#include <d3d12.h>

#include "common.h"

namespace gfx {
    struct Device;
    struct RenderPass;

    struct Pipeline {
        explicit Pipeline(const Device& device, const std::string& vertex_shader, const std::string& pixel_shader);

    public:
        ComPtr<ID3D12PipelineState> pipeline_state;
        ComPtr<ID3D12RootSignature> root_signature;
    };
}
