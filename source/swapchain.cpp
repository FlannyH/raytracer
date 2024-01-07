#include "swapchain.h"
#include "device.h"

namespace gfx {
    Swapchain::Swapchain(const Device& device, const CommandQueue& queue) {
        _frame_index = 0;

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
            _swapchain = static_cast<IDXGISwapChain3*>(new_swapchain);
        }

        if (!_swapchain) {
            throw std::exception();
        }
    }
}