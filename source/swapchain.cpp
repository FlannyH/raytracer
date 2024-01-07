#include "swapchain.h"
#include "device.h"
#include "descriptor_heap.h"
#include "command.h"

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
            const auto rtv_handle = rtv_heap.alloc_descriptor();

            validate(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_render_targets[i])));
            device.device->CreateRenderTargetView(m_render_targets[i].Get(), nullptr, rtv_handle);
        }

        m_frame_index = 0;
    }
}