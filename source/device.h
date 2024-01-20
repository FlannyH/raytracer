#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <memory>

#include "common.h"
#include "glfw/glfw3.h"
#include "glm/matrix.hpp"
#include "resource.h"

namespace gfx {
    struct Swapchain;
    struct CommandQueue;
    struct DescriptorHeap;
    struct RenderPass;
    struct Pipeline;

    struct Device {
        Device(int width, int height, bool debug_layer_enabled);
        void resize_window(int width, int height) const;
        void get_window_size(int& width, int& height) const;
        void init_context();
        std::shared_ptr<RenderPass> create_render_pass();
        std::shared_ptr<Pipeline> create_raster_pipeline(const RenderPass& render_pass); // todo: give the user the option to specify custom shaders
        void begin_frame();
        void end_frame();
        ResourceID load_bindless_texture(const std::string& path);
        ResourceID load_bindless_texture(uint32_t width, uint32_t height, void* data, PixelFormat pixel_format);
        void unload_bindless_resource(ResourceID id);

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