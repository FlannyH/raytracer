#pragma once
#include <d3d12.h>
#include <deque>
#include <dxgi1_6.h>
#include <map>
#include <memory>
#include <unordered_map>
#include <array>
#include <queue>

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
    struct Transform;

    struct RasterPassInfo {
        std::vector<ResourceHandle> color_targets; // If empty, it will instead use the swapchain framebuffer as a color target
        ResourceHandle depth_target; // Optional; passing `ResourceHandle::none()` will disable depth testing
        bool clear_on_begin = true;
    };

    struct Device {
    public:
        // Initialization
        Device(int width, int height, bool debug_layer_enabled);
        void resize_window(int width, int height) const;
        void get_window_size(int& width, int& height) const;

        // Common rendering
        bool should_stay_open();
        void set_full_screen(bool full_screen);
        void begin_frame();
        void end_frame();
        void set_root_constants(const std::vector<uint32_t>& constants);

        // Rasterization
        std::shared_ptr<Pipeline> create_raster_pipeline(const std::string& vertex_shader_path, const std::string& pixel_shader_path);
        void begin_raster_pass(std::shared_ptr<Pipeline> pipeline, RasterPassInfo&& render_pass_info);
        void end_raster_pass();
        size_t create_draw_packet(const void* data, const size_t size_bytes); // Returns the byte offset into the `m_draw_packets` buffer where this new draw packet was allocated
        void draw_vertices(uint32_t n_vertices);
        void draw_scene(ResourceHandle scene_handle);

        // Resource management
        ResourceHandlePair load_texture(const std::string& path); // Load a texture from a file
        ResourceHandlePair load_texture(const std::string& name, uint32_t width, uint32_t height, void* data, PixelFormat pixel_format); // Load a texture from memory
        ResourceHandlePair load_mesh(const std::string& name, uint64_t n_triangles, Triangle* tris);
        ResourceHandlePair create_scene_graph_from_gltf(const std::string& path);
        ResourceHandlePair create_buffer(const std::string& name, size_t size, void* data);
        ResourceHandlePair create_render_target(const std::string& name, uint32_t width, uint32_t height, PixelFormat pixel_format, glm::vec4 clear_color = { 0.0f, 0.0f, 0.0f, 1.0f });
        ResourceHandlePair create_depth_target(const std::string& name, uint32_t width, uint32_t height, PixelFormat pixel_format, float clear_value = 1.0f);
        void resize_texture(ResourceHandle& texture, const uint32_t width, const uint32_t height);
        void unload_bindless_resource(ResourceHandle id);
        void transition_resource(CommandBuffer* cmd, Resource* resource, D3D12_RESOURCE_STATES new_state);

        ComPtr<ID3D12Device> device = nullptr;
        ComPtr<IDXGIFactory4> factory = nullptr;
        HWND window_hwnd = nullptr;

    private:
        int find_dominant_monitor(); // Returns the index of the monitor the window overlaps with most
        void clean_up_old_resources();
        void traverse_scene(SceneNode* node);

        // Device
        GLFWwindow* m_window_glfw = nullptr;
        ComPtr<ID3D12Debug1> m_debug_layer = nullptr;
        ComPtr<ID3D12DebugDevice> m_device_debug = nullptr;
        std::shared_ptr<DescriptorHeap> m_heap_rtv = nullptr;
        std::shared_ptr<DescriptorHeap> m_heap_dsv = nullptr;
        std::shared_ptr<DescriptorHeap> m_heap_bindless = nullptr;
        std::shared_ptr<CommandQueue> m_queue_gfx = nullptr;

        // Swapchain
        std::shared_ptr<Swapchain> m_swapchain = nullptr;
        int m_width = 0;
        int m_height = 0;
        int m_width_pre_fullscreen = 0;
        int m_height_pre_fullscreen = 0;
        int m_pos_x_pre_fullscreen = 0;
        int m_pos_y_pre_fullscreen = 0;
        bool m_is_fullscreen = false;
        PixelFormat m_framebuffer_format = PixelFormat::rgba_8;

        // Resource management
        std::unordered_map<uint64_t, std::shared_ptr<Resource>> m_resources; // Maps linking resource IDs and actual resource data
        std::unordered_map<std::string, ResourceHandle> m_resource_name_map; // Maps resources to their names so we don't load in duplicates
        std::deque<std::pair<ResourceHandlePair, int>> m_resources_to_unload; // Resources to unload. The integer determines when it should be unloaded
        std::shared_ptr<CommandQueue> m_upload_queue = nullptr; // Command queue for uploading resources to the GPU
        size_t m_upload_fence_value_when_done = 0; // The value the upload queue fence will signal when it's done uploading

        // Rendering context
        std::shared_ptr<Pipeline> m_curr_bound_pipeline = nullptr; // Will point to a valid pipeline after calling begin_render_pass(), and will be null after calling end_render_pass()
        std::shared_ptr<CommandBuffer> m_curr_pass_cmd; // The command buffer used for this pass
        ResourceHandlePair m_draw_packets{}; // Scratch buffer that is used to send draw info to the shader
        size_t m_draw_packet_cursor = 0;
        size_t m_camera_matrices_offset = 0;
        std::vector<std::shared_ptr<Resource>> m_curr_render_targets;
        std::shared_ptr<Resource> m_curr_depth_target = nullptr;
    };
};
