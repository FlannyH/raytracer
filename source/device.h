#pragma once
#include <d3d12.h>
#include <deque>
#include <dxgi1_6.h>
#include <map>
#include <memory>
#include <unordered_map>
#include <array>
#include <queue>
#include <thread>

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
    struct Fence;

    struct RasterPassInfo {
        std::vector<ResourceHandlePair> color_targets; // If empty, it will instead use the swapchain framebuffer as a color target
        ResourceHandlePair depth_target; // Optional; passing `ResourceHandle::none()` will disable depth testing
        bool clear_on_begin = true;
    };

    struct UploadQueueKeepAlive {
        size_t upload_queue_fence_value; // If the upload queue fence has this value, the resource has been uploaded, the upload buffer can be removed, and the handle's `is_loaded` flag should be set
        std::shared_ptr<Resource> upload_buffer; // Temporary buffer that will be copied to the destination resource
    };

    struct Device {
    public:
        // Initialization
        Device(int width, int height, bool debug_layer_enabled);
        ~Device();
        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;
        void resize_window(int width, int height) const;
        void get_window_size(int& width, int& height) const;

        // Common rendering
        bool should_stay_open();
        void set_full_screen(bool full_screen);
        void begin_frame();
        void end_frame();
        void set_graphics_root_constants(const std::vector<uint32_t>& constants);
        void set_compute_root_constants(const std::vector<uint32_t>& constants);
        int frame_index();

        // Rasterization
        std::shared_ptr<Pipeline> create_raster_pipeline(const std::string& vertex_shader_path, const std::string& pixel_shader_path, const std::initializer_list<ResourceHandlePair> render_targets, const ResourceHandlePair depth_target = { ResourceHandle::none(), nullptr });
        void begin_raster_pass(std::shared_ptr<Pipeline> pipeline, RasterPassInfo&& render_pass_info);
        void end_raster_pass();
        void draw_vertices(uint32_t n_vertices);

        // Compute
        std::shared_ptr<Pipeline> create_compute_pipeline(const std::string& compute_shader_path);
        void begin_compute_pass(std::shared_ptr<Pipeline> pipeline, bool async = false);
        void end_compute_pass();
        void dispatch_threadgroups(uint32_t x, uint32_t y, uint32_t z);

        // Resource management
        ResourceHandlePair load_texture(const std::string &name, uint32_t width, uint32_t height, uint32_t depth, void *data, PixelFormat pixel_format, TextureType type, ResourceUsage usage = ResourceUsage::none, bool generate_mips = false); // Load a texture from memory
        ResourceHandlePair load_mesh(const std::string& name, uint64_t n_triangles, Triangle* tris);
        ResourceHandlePair create_buffer(const std::string& name, size_t size, void* data, bool cpu_visible, ResourceUsage usage = ResourceUsage::none);
        ResourceHandlePair create_render_target(const std::string& name, uint32_t width, uint32_t height, PixelFormat pixel_format, std::optional<glm::vec4> clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), ResourceUsage extra_usage = ResourceUsage::none);
        ResourceHandlePair create_depth_target(const std::string& name, uint32_t width, uint32_t height, PixelFormat pixel_format, float clear_value = 1.0f);
        void resize_texture(ResourceHandlePair& texture, const uint32_t width, const uint32_t height);
        void update_buffer(const ResourceHandlePair& buffer, const uint32_t offset, const uint32_t length, const void* data);
        void queue_unload_bindless_resource(ResourceHandlePair resource);
        void use_resource(const ResourceHandlePair& resource, const ResourceUsage usage = ResourceUsage::read);
        void use_resources(const std::initializer_list<std::pair<ResourceHandlePair, ResourceUsage>>& resources);

        ComPtr<ID3D12Device> device = nullptr;
        ComPtr<IDXGIFactory4> factory = nullptr;
        HWND window_hwnd = nullptr;

    private:
        void transition_resource(std::shared_ptr<CommandBuffer> cmd, std::shared_ptr<Resource> resource, D3D12_RESOURCE_STATES new_state);
        int find_dominant_monitor(); // Returns the index of the monitor the window overlaps with most
        void clean_up_old_resources();
        void execute_resource_transitions(std::shared_ptr<CommandBuffer> cmd);

        // Device
        GLFWwindow* m_window_glfw = nullptr;
        ComPtr<ID3D12Debug1> m_debug_layer = nullptr;
        ComPtr<ID3D12DebugDevice> m_device_debug = nullptr;
        std::shared_ptr<DescriptorHeap> m_heap_rtv = nullptr;
        std::shared_ptr<DescriptorHeap> m_heap_dsv = nullptr;
        std::shared_ptr<DescriptorHeap> m_heap_bindless = nullptr;
        std::shared_ptr<CommandQueue> m_queue_gfx = nullptr;
        std::thread device_lost_thread;
        DWORD m_msg_callback_cookie = 0;

        // Swapchain
        std::shared_ptr<Swapchain> m_swapchain = nullptr;
        int m_width = 0;
        int m_height = 0;
        int m_width_pre_fullscreen = 0;
        int m_height_pre_fullscreen = 0;
        int m_pos_x_pre_fullscreen = 0;
        int m_pos_y_pre_fullscreen = 0;
        bool m_is_fullscreen = false;
        PixelFormat m_framebuffer_format = PixelFormat::rgba8_unorm;

        // Resource management
        std::shared_ptr<CommandQueue> m_upload_queue = nullptr; // Command queue for uploading resources to the GPU
        std::shared_ptr<Fence> m_upload_queue_completion_fence = nullptr;
        size_t m_upload_fence_value_when_done = 0; // The value the upload queue fence will signal when it's done uploading
        std::deque<UploadQueueKeepAlive> m_temp_upload_buffers; // Temporary upload buffer to be unloaded after it's done uploading. The integer is upload queue fence value before it should be unloaded
        std::deque<std::pair<ResourceHandlePair, int>> m_resources_to_unload; // Resources to unload. The integer determines when it should be unloaded
        std::vector<D3D12_RESOURCE_BARRIER> m_resource_barriers; // Enqueued resource barriers

        // Rendering context
        std::shared_ptr<Pipeline> m_curr_bound_pipeline = nullptr; // Will point to a valid pipeline after calling begin_render_pass(), and will be null after calling end_render_pass()
        std::shared_ptr<CommandBuffer> m_curr_pass_cmd; // The command buffer used for this pass
        std::shared_ptr<CommandBuffer> m_queue_async; // The command queue used for async compute passes
        std::shared_ptr<Resource> m_curr_depth_target = nullptr; // Currently bound depth target - keeping track of them for proper resource transitions at the end of a render pass
        std::vector<std::shared_ptr<Resource>> m_pass_resources; // Resources used in the current pass, which we will need to transition back to a common state at the end of the pass
        bool m_curr_pipeline_is_async = false; // If the current pipeline is async, we need to keep track of resources differently 
    };
};
