#include "swapchain.h"

#include "command_buffer.h"
#include "device.h"
#include "descriptor_heap.h"
#include "command_queue.h"
#include "fence.h"

namespace gfx {
    Swapchain::Swapchain(const Device& device, const CommandQueue& queue, DescriptorHeap& rtv_heap) {
        device.get_window_size(m_width, m_height);

        const DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {
            .Width = static_cast<UINT>(m_width),
            .Height = static_cast<UINT>(m_height),
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
            m_render_target_views[i] = rtv_heap.fetch_cpu_handle(rtv_id);

            validate(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_render_targets[i])));
            device.device->CreateRenderTargetView(m_render_targets[i].Get(), nullptr, m_render_target_views[i]);
        }

        m_fence = std::make_shared<Fence>(device);

        m_frame_index = framebuffer_index() - 1;
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

        const auto viewport = D3D12_VIEWPORT {
            .TopLeftX = 0.0f,
            .TopLeftY = 0.0f,
            .Width = static_cast<float>(m_width),
            .Height = static_cast<float>(m_height),
            .MinDepth = 0.0f,
            .MaxDepth = 1.0f,
        };

        const auto scissor_rect = D3D12_RECT {
            .left = 0,
            .top = 0,
            .right = m_width,
            .bottom = m_height,
        };

        const float clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

        auto cmd = command_buffer->expect_graphics_command_list();
        cmd->ResourceBarrier(1, &render_target_barrier);
        cmd->OMSetRenderTargets(1, &m_render_target_views[framebuffer_index()], FALSE, nullptr);
        cmd->RSSetViewports(1, &viewport);
        cmd->RSSetScissorRects(1, &scissor_rect);
        cmd->ClearRenderTargetView(m_render_target_views[framebuffer_index()], clear_color, 1, &scissor_rect);
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
