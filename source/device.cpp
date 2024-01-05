#define GLFW_EXPOSE_NATIVE_WIN32

#include "device.h"
#include <glfw/glfw3.h>
#include <glfw/glfw3native.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <exception>
#include <wrl.h>
#include "common.h"

using Microsoft::WRL::ComPtr;

// Data
namespace gfx
{
    GLFWwindow* _window_glfw;
    HWND _window_hwnd;
    ComPtr<ID3D12Debug1> _debug_layer;
    ComPtr<ID3D12Device> _device;
    ComPtr<ID3D12DebugDevice> _device_debug;
}

// Functions
namespace gfx {
    void create_window(const int width, const int height) {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // use GLFW_NO_API, since we're not using OpenGL
        _window_glfw = glfwCreateWindow(width, height, "Ray Tracer (DirectX 12)", nullptr, nullptr);
        _window_hwnd = glfwGetWin32Window(_window_glfw);
    }

    void resize_window(const int width, const int height) {
        glfwSetWindowSize(_window_glfw, width, height);
    }

    void get_window_size(int& width, int& height) {
        glfwGetWindowSize(_window_glfw, &width, &height);
    }

    ComPtr<ID3D12Device> device() {
        return device;
    }

    void init_device(const bool debug_layer_enabled) {
        ComPtr<IDXGIFactory4> factory;
        UINT dxgi_factory_flags = 0;

        if (debug_layer_enabled) {
            // If we're in debug mode, create a debug layer for proper error tracking
            // Note: Errors will be printed in the Visual Studio output tab, and not in the console!
            validate(D3D12GetDebugInterface(IID_PPV_ARGS(&_debug_layer)));
            validate(_debug_layer->QueryInterface(IID_PPV_ARGS(&_debug_layer)));
            _debug_layer->EnableDebugLayer();
            _debug_layer->SetEnableGPUBasedValidation(true);
            dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
            _debug_layer->Release();
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
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&_device)))) {
                // Yes it does! We use this one.
                break;
            }

            // It doesn't? Unfortunate, let's keep looking
            _device = nullptr;
            adapter->Release();
            adapter_index++;
        }

        if (_device == nullptr) {
            throw std::exception();
        }

        if (debug_layer_enabled) {
            validate(_device->QueryInterface(_device_debug.GetAddressOf()));
        }
    }
}