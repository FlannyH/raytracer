#include "device.h"
#include <glfw/glfw3.h>
#include <glfw/glfw3native.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include "common.h"

namespace gfx {
    extern ComPtr<IDXGIFactory4> _factory;
    extern ComPtr<ID3D12CommandQueue> _command_queue;
    extern HWND _window_hwnd;

    constexpr UINT backbuffer_count = 3;
    ComPtr<IDXGISwapChain3> _swapchain = nullptr;
    ComPtr<ID3D12Resource> _render_targets[backbuffer_count];
    int _frame_wait_values[backbuffer_count];
}

namespace gfx {
    void create_swapchain() {
        int width, height;
        gfx::get_window_size(width, height);

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
        validate(_factory->CreateSwapChainForHwnd(
            _command_queue.Get(), 
            _window_hwnd, 
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