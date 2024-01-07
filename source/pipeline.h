#pragma once
#include <d3d12.h>

#include "common.h"

namespace gfx {
    struct Device;
    struct RenderPass;

    struct Pipeline {
        explicit Pipeline(const Device& device, const RenderPass& render_pass);

    public:
        ComPtr<ID3D12PipelineState> pipeline_state;
    };
}
