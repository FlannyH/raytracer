#pragma once
#include <d3d12.h>

#include "common.h"

namespace gfx {
    struct Device;
    struct RenderPass;

    struct Pipeline {
        explicit Pipeline(const Device& device, const std::string& name, const std::string& vertex_shader_path, const std::string& pixel_shader_path, const std::vector<DXGI_FORMAT> render_target_formats, const DXGI_FORMAT depth_target_format = DXGI_FORMAT_UNKNOWN);
        explicit Pipeline(const Device& device, const std::string& name, const std::string& compute_shader_path);
        const std::string& get_name() { return name; }

    public:
        ComPtr<ID3D12PipelineState> pipeline_state;
        ComPtr<ID3D12RootSignature> root_signature;
        std::string name;

    };
}
