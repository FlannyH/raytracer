#pragma once
#include <cstdio>
#include <d3d12.h>
#include <dxgi1_4.h>

#include "common.h"

namespace gfx {
    struct Device;
    struct CommandQueue;

    struct Swapchain {
        Swapchain(const Device& device, const CommandQueue& queue);

    private:    
        ComPtr<IDXGISwapChain3> m_swapchain = nullptr;
        ComPtr<ID3D12Resource> m_render_targets[backbuffer_count] = {};
        int m_frame_wait_values[backbuffer_count]{};
        int m_frame_index;
    };
}
