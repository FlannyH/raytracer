#pragma once
#include <cstdio>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <memory>

#include "common.h"

namespace gfx {
    struct Fence;
    struct DescriptorHeap;
    struct Device;
    struct CommandQueue;

    struct Swapchain {
        Swapchain(const Device& device, const CommandQueue& queue, DescriptorHeap& rtv_heap);

    private:    
        ComPtr<IDXGISwapChain3> m_swapchain = nullptr;
        ComPtr<ID3D12Resource> m_render_targets[backbuffer_count] = {};
        std::shared_ptr<Fence> m_fence;
        int m_frame_wait_values[backbuffer_count]{};
        int m_frame_index;
    };
}
