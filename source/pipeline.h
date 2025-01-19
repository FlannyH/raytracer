#pragma once
#include <d3d12.h>
#include <optional>

#include "common.h"

namespace gfx {
    struct Device;
    struct RenderPass;

    struct HitGroup {
        std::optional<std::string> intersection_shader_path;
        std::optional<std::string> closest_hit_shader_path;
        std::optional<std::string> any_hit_shader_path;
    };

    struct Pipeline {
        void create_global_root_signature(const gfx::Device& device);
        explicit Pipeline(const Device& device, const std::string& name, const std::string& vertex_shader_path, const std::string& pixel_shader_path, const std::vector<DXGI_FORMAT> render_target_formats, const DXGI_FORMAT depth_target_format = DXGI_FORMAT_UNKNOWN);
        explicit Pipeline(const Device& device, const std::string& name, const std::string& compute_shader_path);
        explicit Pipeline(const Device& device, const std::string& name, const std::string& raygen_shader_path, const std::string& miss_shader_path, std::vector<HitGroup>& hit_groups);
        const std::string& get_name() { return name; }

    public:
        ComPtr<ID3D12PipelineState> pipeline_state;
        ComPtr<ID3D12RootSignature> root_signature;
        std::string name;

    };
}
