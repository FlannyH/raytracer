#define GLFW_EXPOSE_NATIVE_WIN32
#define STB_IMAGE_IMPLEMENTATION

#include "device.h"
#include <glfw/glfw3native.h>
#include <stb/stb_image.h>

#include "command_buffer.h"
#include "swapchain.h"
#include "render_pass.h"
#include "pipeline.h"
#include "descriptor_heap.h"
#include "command_queue.h"

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

    std::shared_ptr<RenderPass> Device::create_render_pass() {
        return std::make_shared<RenderPass>(*this);
    }

    std::shared_ptr<Pipeline> Device::create_raster_pipeline(const RenderPass& render_pass) {
        return std::make_shared<Pipeline>(*this, render_pass);
    }

    void Device::begin_frame() {
    }

    void Device::end_frame() {
    }

    ResourceID Device::load_bindless_texture(const std::string& path) {
        int width, height, channels;
        uint8_t* data = stbi_load(path.c_str(), &width, &height, &channels, 4);

        Resource resource {
            .type = ResourceType::texture,
            .texture_resource = {
                .width = static_cast<uint32_t>(width),
                .height = static_cast<uint32_t>(height),
                .pixel_format = DXGI_FORMAT_R8G8B8A8_UNORM,
                .data = data
            }
        };

        D3D12_RESOURCE_DESC resource_desc = {};
        resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resource_desc.Width = resource.texture_resource.width;
        resource_desc.Height = resource.texture_resource.height;
        resource_desc.DepthOrArraySize = 1;
        resource_desc.MipLevels = 1;
        resource_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        resource_desc.SampleDesc.Count = 1;
        resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES heap_properties = {};
        heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;

        auto id = m_heap_bindless->alloc_descriptor(ResourceType::texture);
        auto descriptor = m_heap_bindless->fetch_cpu_handle(id);
        device->CreateCommittedResource(
            &heap_properties,
            D3D12_HEAP_FLAG_NONE,
            &resource_desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&resource.handle)
        );

        id.is_loaded = true;
        return id;
    }

    void Device::unload_bindless_resource(ResourceID id) {
        m_heap_bindless->free_descriptor(id);
    }
}
