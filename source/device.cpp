#define GLFW_EXPOSE_NATIVE_WIN32

#include "device.h"
#include <glfw/glfw3native.h>

#include "swapchain.h"

namespace gfx {
    Device::Device(const int width, const int height, const bool debug_layer_enabled) {
        // Create window
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // use GLFW_NO_API, since we're not using OpenGL
        m_window_glfw = glfwCreateWindow(width, height, "Ray Tracer (DirectX 12)", nullptr, nullptr);
        window_hwnd = glfwGetWin32Window(m_window_glfw);

        UINT dxgi_factory_flags = 0;

        if (debug_layer_enabled) {
            // If we're in debug mode, create a debug layer for proper error tracking
            // Note: Errors will be printed in the Visual Studio output tab, and not in the console!
            dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
            validate(D3D12GetDebugInterface(IID_PPV_ARGS(&m_debug_layer)));
            validate(m_debug_layer->QueryInterface(IID_PPV_ARGS(&m_debug_layer)));
            m_debug_layer->EnableDebugLayer();
            m_debug_layer->SetEnableGPUBasedValidation(true);
            m_debug_layer->Release();
        }

        // Create factory
        validate(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&factory)));

        // Find adapter
        ComPtr<IDXGIAdapter1> adapter;
        UINT adapter_index = 0;
        while (factory->EnumAdapters1(adapter_index, &adapter) != DXGI_ERROR_NOT_FOUND) {
            DXGI_ADAPTER_DESC1 desc;
            validate(adapter->GetDesc1(&desc));

            // Ignore software renderer
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                continue;
            }

            // Does this adapter support Direct3D 12.0?
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)))) {
                // Yes it does! We use this one.
                break;
            }

            // It doesn't? Unfortunate, let's keep looking
            device = nullptr;
            adapter->Release();
            adapter_index++;
        }

        if (device == nullptr) {
            throw std::exception();
        }

        if (debug_layer_enabled) {
            validate(device->QueryInterface(m_device_debug.GetAddressOf()));
        }

        // Create descriptor heaps
        m_heap_rtv = std::make_shared<DescriptorHeap>(*this, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, backbuffer_count);
        m_heap_bindless = std::make_shared<DescriptorHeap>(*this, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1);
    }


    void Device::resize_window(const int width, const int height) const {
        glfwSetWindowSize(m_window_glfw, width, height);
    }

    void Device::get_window_size(int& width, int& height) const {
        glfwGetWindowSize(m_window_glfw, &width, &height);
    }

    void Device::init_context() {
        m_queue = std::make_shared<CommandQueue>(*this);
        m_swapchain = std::make_shared<Swapchain>(*this, *m_queue, *m_heap_rtv);
    }
}
