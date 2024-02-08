#pragma once
#include <d3d12.h>
#include <deque>
#include <dxgi1_6.h>
#include <map>
#include <memory>
#include <unordered_map>

#include "common.h"
#include "glfw/glfw3.h"
#include "glm/matrix.hpp"
#include "resource.h"

namespace gfx {
    struct CommandBuffer;
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
        std::shared_ptr<Pipeline> create_raster_pipeline(const RenderPass& render_pass, const std::string& vertex_shader, const std::string& pixel_shader);
        void begin_frame();
        void end_frame();
        void test(std::shared_ptr<Pipeline> pipeline, std::shared_ptr<RenderPass>, ResourceID bindings);
        ResourceID load_bindless_texture(const std::string& path);
        ResourceID load_bindless_texture(const std::string& name, uint32_t width, uint32_t height, void* data, PixelFormat pixel_format);
        ResourceID load_mesh(const std::string& name, uint64_t n_triangles, Triangle* tris);
        ResourceID create_buffer(const std::string& name, size_t size, void* data);
        void unload_bindless_resource(ResourceID id);
        bool should_stay_open();

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
        std::unordered_map<std::string, ResourceID> m_resource_name_map; // so we don't load in duplicates
        std::unordered_map<uint64_t, std::shared_ptr<Resource>> m_resources;
        std::shared_ptr<CommandQueue> m_upload_queue = nullptr;
        std::deque<std::shared_ptr<CommandBuffer>> m_upload_cmd;
        size_t m_upload_fence_value_when_done = 0;
    };
};