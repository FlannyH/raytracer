#include "swapchain.h"

#include "command_buffer.h"
#include "device.h"
#include "descriptor_heap.h"
#include "command_queue.h"
#include "fence.h"

namespace gfx {
    Swapchain::Swapchain(const Device& device, const CommandQueue& queue, DescriptorHeap& rtv_heap) {
        int width, height;
        device.get_window_size(width, height);

        const DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {
            .Width = static_cast<UINT>(width),
            .Height = static_cast<UINT>(height),
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .SampleDesc = {
                .Count = 1,
            },
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = backbuffer_count,
            .Scaling = DXGI_SCALING_STRETCH,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        };

        IDXGISwapChain1* new_swapchain;
        validate(device.factory->CreateSwapChainForHwnd(
            queue.command_queue.Get(),
            device.window_hwnd, 
            &swapchain_desc, 
            nullptr, 
            nullptr, 
            &new_swapchain
        ));

        const HRESULT swapchain_support = new_swapchain->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&new_swapchain));

        if (SUCCEEDED(swapchain_support)) {
            m_swapchain = static_cast<IDXGISwapChain3*>(new_swapchain);
        }

        if (!m_swapchain) {
            throw std::exception();
        }

        // Create render targets
        for (UINT i = 0; i < backbuffer_count; i++) {
            // Allocate descriptor
            const auto rtv_id = rtv_heap.alloc_descriptor(ResourceType::texture);
            const auto rtv_handle = rtv_heap.fetch_cpu_handle(rtv_id);

            validate(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_render_targets[i])));
            device.device->CreateRenderTargetView(m_render_targets[i].Get(), nullptr, rtv_handle);
        }

        m_fence = std::make_shared<Fence>(device);

        m_frame_index = 0;
    }

    ComPtr<ID3D12Resource> Swapchain::next_framebuffer() {
        m_frame_index++;
        m_fence->cpu_wait(m_frame_wait_values[framebuffer_index()]);
        return m_render_targets[framebuffer_index()];
    }

    void Swapchain::prepare_render(std::shared_ptr<CommandBuffer> command_buffer) {
        D3D12_RESOURCE_BARRIER render_target_barrier;
        render_target_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        render_target_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        render_target_barrier.Transition.pResource = m_render_targets[framebuffer_index()].Get();
        render_target_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        render_target_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        render_target_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        auto cmd = command_buffer->expect_graphics_command_list();
        cmd->ResourceBarrier(1, &render_target_barrier);
    }

    void Swapchain::present() {
        m_swapchain->Present(0, 0);
    }

    void Swapchain::prepare_present(std::shared_ptr<CommandBuffer> command_buffer) {
        D3D12_RESOURCE_BARRIER present_barrier;
        present_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        present_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        present_barrier.Transition.pResource = m_render_targets[framebuffer_index()].Get();
        present_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        present_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        present_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        auto cmd = command_buffer->expect_graphics_command_list();
        cmd->ResourceBarrier(1, &present_barrier);
    }

    void Swapchain::synchronize(std::shared_ptr<CommandQueue> queue) {
        m_fence->gpu_signal(queue, m_frame_index);
        m_frame_wait_values[framebuffer_index()] = m_frame_index;
    }
}
