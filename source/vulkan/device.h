#pragma once

#include <deque>
#include <map>
#include <memory>
#include <unordered_map>
#include <array>
#include <queue>
#include <thread>

#include "../common.h"
#include <glfw/glfw3.h>
#include <glm/matrix.hpp>
#include "../resource.h"
#include "../device.h"

namespace gfx::vk {
    struct CommandBuffer;
    struct Swapchain;
    struct CommandQueue;
    struct DescriptorHeap;
    struct RenderPass;
    struct Pipeline;
    struct Transform;
    struct Fence;

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphics_family;
        std::optional<uint32_t> compute_family;
    };

    struct ResourceInfo {
        VkAccessFlags access_mask = VK_ACCESS_FLAG_BITS_MAX_ENUM;
        VkImageLayout image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        uint32_t queue_family_index = 0xFFFFFFFF;

        // Optional Vk handles
        VkBuffer buffer;
        VkImage image;
        VkImageView image_view;
    };

    struct Device : public gfx::Device {
    public:
        // Initialization
        Device(int width, int height, bool debug_layer_enabled, bool gpu_profiling_enabled);
        ~Device();
        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;
        void resize_window(int width, int height) const override;
        void get_window_size(int& width, int& height) const override;

        // Common rendering
        bool should_stay_open() override;
        void set_full_screen(bool full_screen) override;
        void begin_frame() override;
        void end_frame() override;
        void set_graphics_root_constants(const std::vector<uint32_t>& constants) override;
        void set_compute_root_constants(const std::vector<uint32_t>& constants) override;
        int frame_index() override;
        bool supports(RendererFeature feature) override;

        // Rasterization
        PipelineHandle create_raster_pipeline(const std::string& name, const std::string& vertex_shader_path, const std::string& pixel_shader_path, const std::initializer_list<ResourceHandlePair> render_targets, const ResourceHandlePair depth_target = { ResourceHandle::none(), nullptr }) override;
        void begin_raster_pass(PipelineHandle pipeline, RasterPassInfo&& render_pass_info) override;
        void end_raster_pass() override;
        void draw_vertices(uint32_t n_vertices) override;

        // Compute
        PipelineHandle create_compute_pipeline(const std::string& name, const std::string& compute_shader_path) override;
        void begin_compute_pass(PipelineHandle pipeline, bool async = false) override;
        void end_compute_pass() override;
        void dispatch_threadgroups(uint32_t x, uint32_t y, uint32_t z) override;

        // Resource management
        ResourceHandlePair load_texture(const std::string &name, uint32_t width, uint32_t height, uint32_t depth, void *data, PixelFormat pixel_format, TextureType type, ResourceUsage usage = ResourceUsage::none, int max_mip_levels = 1, int min_resolution = 1) override; // Load a texture from memory
        ResourceHandlePair load_mesh(const std::string& name, uint64_t n_triangles, Triangle* tris) override;
        ResourceHandlePair create_buffer(const std::string& name, size_t size, void* data, ResourceUsage usage = ResourceUsage::none) override;
        ResourceHandlePair create_render_target(const std::string& name, uint32_t width, uint32_t height, PixelFormat pixel_format, std::optional<glm::vec4> clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), ResourceUsage extra_usage = ResourceUsage::none) override;
        ResourceHandlePair create_depth_target(const std::string& name, uint32_t width, uint32_t height, PixelFormat pixel_format, float clear_value = 1.0f) override;
        void resize_texture(ResourceHandlePair& texture, const uint32_t width, const uint32_t height) override;
        void update_buffer(const ResourceHandlePair& buffer, const uint32_t offset, const uint32_t n_bytes, const void* data) override;
        void readback_buffer(const ResourceHandlePair& buffer, const uint32_t offset, const uint32_t n_bytes, void* destination) override;
        void queue_unload_bindless_resource(ResourceHandlePair resource) override;
        void use_resource(const ResourceHandle handle, const ResourceUsage usage = ResourceUsage::read) override;
        void use_resources(const std::initializer_list<ResourceTransitionInfo>& resources) override;

        // Raytracing resources
        ResourceHandlePair create_acceleration_structure(const std::string& name, const size_t size) override;
        ResourceHandlePair create_blas(const std::string& name, const ResourceHandlePair& position_buffer, const ResourceHandlePair& index_buffer, const uint32_t vertex_count, const uint32_t index_count) override;
        ResourceHandlePair create_tlas(const std::string& name, const std::vector<RaytracingInstance>& instances) override;

        const QueueFamilyIndices& queue_family_indices() const { return m_indices; }

        HWND window_hwnd = nullptr;
        VkDevice device;

    private:        
        void transition_resource(VkCommandBuffer cmd, ResourceHandlePair resource, ResourceInfo&& new_state, VkImageSubresourceRange subresource_range = {});
        void execute_resource_transitions(VkCommandBuffer cmd, VkPipelineStageFlags source, VkPipelineStageFlags destination);
        ResourceInfo& fetch_resource_info(ResourceHandle handle);

        VkPhysicalDevice m_physical_device;
        GLFWwindow* m_window_glfw = nullptr;
        QueueFamilyIndices m_indices{};
        
        std::shared_ptr<CommandQueue> m_queue_compute = nullptr;
        std::shared_ptr<CommandQueue> m_queue_graphics = nullptr;

        std::shared_ptr<DescriptorHeap> m_desc_heap = nullptr;
        VkSampler m_samplers[3];

        std::unordered_map<uint32_t, ResourceInfo> m_resource_info;
        std::vector<VkImageMemoryBarrier> m_queued_image_memory_barriers;
        std::vector<VkBufferMemoryBarrier> m_queued_buffer_memory_barriers;

        size_t m_upload_fence_value_when_done = 0;
    };

    static_assert((uint32_t)RaytracingInstanceFlags::triangle_cull_disable            == (uint32_t)VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR);
    static_assert((uint32_t)RaytracingInstanceFlags::triangle_front_counterclockwise  == (uint32_t)VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR);
    static_assert((uint32_t)RaytracingInstanceFlags::force_opaque                     == (uint32_t)VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR);
    static_assert((uint32_t)RaytracingInstanceFlags::force_non_opaque                 == (uint32_t)VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR);
};
