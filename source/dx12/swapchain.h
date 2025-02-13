#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <memory>

#include "../common.h"
#include "../resource.h"
#include "fence.h"

namespace gfx::dx12 {
    struct CommandBuffer;
    struct Fence;
    struct DescriptorHeap;
    struct Device;
    struct CommandQueue;

    struct Swapchain {
        Swapchain(const Device& device, const CommandQueue& queue, DescriptorHeap& rtv_heap, PixelFormat format);
        ComPtr<ID3D12Resource> next_framebuffer();
        ComPtr<ID3D12Resource> curr_framebuffer();
        D3D12_CPU_DESCRIPTOR_HANDLE curr_framebuffer_rtv();
        void prepare_render(std::shared_ptr<CommandBuffer> command_buffer);
        void present();
        void prepare_present(std::shared_ptr<CommandBuffer> command_buffer);
        void synchronize(std::shared_ptr<CommandQueue> queue);
        void flush(std::shared_ptr<CommandQueue> queue);
        void resize(Device& device, std::shared_ptr<CommandQueue> queue, DescriptorHeap& rtv_heap, uint32_t width, uint32_t height, PixelFormat format);
        void get_back_buffers(const Device& device, DescriptorHeap& rtv_heap);
        int width() const { return m_width; }
        int height() const { return m_height; }
        size_t current_frame_index() const {
            return m_frame_index;
        }
        uint64_t current_fence_completed_value() {
            return m_fence->fence->GetCompletedValue();
        }

    private:
        size_t framebuffer_index() const {
            return (size_t)m_swapchain->GetCurrentBackBufferIndex();
        }
        ComPtr<IDXGISwapChain3> m_swapchain = nullptr;
        ComPtr<ID3D12Resource> m_render_targets[backbuffer_count] = {};
        D3D12_CPU_DESCRIPTOR_HANDLE m_render_target_views[backbuffer_count] = {};
        D3D12_RESOURCE_STATES m_render_target_states[backbuffer_count] = {};
        std::shared_ptr<Fence> m_fence;
        size_t m_frame_wait_values[backbuffer_count]{};
        size_t m_frame_index = 0;
        int m_width = 0;
        int m_height = 0;
    };
}
