#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <memory>

#include "common.h"

namespace gfx {
    struct CommandBuffer;
    struct Fence;
    struct DescriptorHeap;
    struct Device;
    struct CommandQueue;

    struct Swapchain {
        Swapchain(const Device& device, const CommandQueue& queue, DescriptorHeap& rtv_heap);
        ComPtr<ID3D12Resource> next_framebuffer();
        void prepare_render(std::shared_ptr<CommandBuffer> command_buffer);
        void present();
        void prepare_present(std::shared_ptr<CommandBuffer> command_buffer);
        void synchronize(std::shared_ptr<CommandQueue> queue);
        int current_frame_index() const {
            return m_frame_index;
        }

    private:
        int framebuffer_index() const {
            return m_swapchain->GetCurrentBackBufferIndex();
        }
        ComPtr<IDXGISwapChain3> m_swapchain = nullptr;
        ComPtr<ID3D12Resource> m_render_targets[backbuffer_count] = {};
        std::shared_ptr<Fence> m_fence;
        int m_frame_wait_values[backbuffer_count]{};
        int m_frame_index;
    };
}
