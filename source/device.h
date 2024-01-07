#pragma once
#include <dxgi1_4.h>
#include <memory>

#include "command.h"
#include "descriptor_heap.h"
#include "swapchain.h"
#include "glfw/glfw3.h"

namespace gfx {
    struct Device {
        Device(int width, int height, bool debug_layer_enabled);
        void resize_window(int width, int height) const;
        void get_window_size(int& width, int& height) const;
        void init_context();

    public:
        ComPtr<ID3D12Device> device = nullptr;
        ComPtr<IDXGIFactory4> factory = nullptr;
        HWND window_hwnd = nullptr;

    private:
        GLFWwindow* m_window_glfw = nullptr;
        ComPtr<ID3D12Debug1> m_debug_layer = nullptr;
        ComPtr<ID3D12DebugDevice> m_device_debug = nullptr;
        std::shared_ptr<DescriptorHeap> m_heap_rtv = nullptr;
        std::shared_ptr<DescriptorHeap> m_heap_bindless = nullptr;
        std::shared_ptr<CommandQueue> m_queue = nullptr;
        std::shared_ptr<Swapchain> m_swapchain = nullptr;
    };
};