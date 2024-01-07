#pragma once
#include <d3d12.h>

#include "common.h"

namespace gfx {
    struct Device;

    struct RenderPass {
        explicit RenderPass(const Device& device);

    public:
        ComPtr<ID3D12RootSignature> root_signature;
    };
}
