#pragma once
#include <d3d12.h>

#include "common.h"

namespace gfx {
    struct Device;
    struct RenderPass;

    struct Pipeline {
        explicit Pipeline(const Device& device, const std::string& vertex_shader, const std::string& pixel_shader, const std::vector<DXGI_FORMAT> render_target_formats, const DXGI_FORMAT depth_target_format = DXGI_FORMAT_UNKNOWN);

    public:
        ComPtr<ID3D12PipelineState> pipeline_state;
        ComPtr<ID3D12RootSignature> root_signature;
    };
}
