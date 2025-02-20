#pragma once

#include <memory>
#include <unordered_map>

#include "resource.h"

#ifdef _WIN32
#include <d3d12.h>
#endif

#include <vulkan/vulkan.h>

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

    struct ResourceTransitionInfo {
        ResourceHandle handle;
        ResourceUsage usage;
        uint32_t subresource_id = (uint32_t)-1;
    };

    enum class RaytracingInstanceFlags : uint32_t {
        none = 0x0,
        triangle_cull_disable = 0x1,
        triangle_front_counterclockwise = 0x2,
        force_opaque = 0x4,
        force_non_opaque = 0x8
    };

    struct RaytracingInstance {
        glm::mat4x3 transform;
        uint32_t instance_id : 24;
        uint32_t instance_mask : 8;
        uint32_t instance_contribution_to_hitgroup_index : 24;
        RaytracingInstanceFlags flags : 8;
        ResourceHandlePair blas;
    };

    enum class RendererFeature : int {
        none =       0,
        raytracing = 1,
    };

    typedef uint32_t PipelineHandle;
    constexpr static PipelineHandle PIPELINE_NULL = 0xFFFFFFFF;

    struct Device {
        // Initialization
        Device() = default;
        virtual ~Device() = default;
        Device(const Device&) = delete;
        virtual Device& operator=(const Device&) = delete;
        virtual void resize_window(int width, int height) const = 0;
        virtual void get_window_size(int& width, int& height) const = 0;

        // Common rendering
        virtual bool should_stay_open() = 0;
        virtual void set_full_screen(bool full_screen) = 0;
        virtual void begin_frame() = 0;
        virtual void end_frame() = 0;
        virtual void set_graphics_root_constants(const std::vector<uint32_t>& constants) = 0;
        virtual void set_compute_root_constants(const std::vector<uint32_t>& constants) = 0;
        virtual int frame_index() = 0;
        virtual bool supports(RendererFeature feature) = 0;

        // Rasterization
        virtual PipelineHandle create_raster_pipeline(const std::string& name, const std::string& vertex_shader_path, const std::string& pixel_shader_path, const std::initializer_list<ResourceHandlePair> render_targets, const ResourceHandlePair depth_target = { ResourceHandle::none(), nullptr }) = 0;        
        virtual void begin_raster_pass(PipelineHandle pipeline, RasterPassInfo&& render_pass_info) = 0;
        virtual void end_raster_pass() = 0;
        virtual void draw_vertices(uint32_t n_vertices) = 0;

        // Compute
        virtual PipelineHandle create_compute_pipeline(const std::string& name, const std::string& compute_shader_path) = 0;
        virtual void begin_compute_pass(PipelineHandle pipeline, bool async = false) = 0;
        virtual void end_compute_pass() = 0;
        virtual void dispatch_threadgroups(uint32_t x, uint32_t y, uint32_t z) = 0;

        // Resource management
        virtual ResourceHandlePair load_texture(const std::string &name, uint32_t width, uint32_t height, uint32_t depth, void *data, PixelFormat pixel_format, TextureType type, ResourceUsage usage = ResourceUsage::none, int max_mip_levels = 1, int min_resolution = 1) = 0; // Load a texture from memory
        virtual ResourceHandlePair load_mesh(const std::string& name, uint64_t n_triangles, Triangle* tris) = 0;
        virtual ResourceHandlePair create_buffer(const std::string& name, size_t size, void* data, ResourceUsage usage = ResourceUsage::none) = 0;
        virtual ResourceHandlePair create_render_target(const std::string& name, uint32_t width, uint32_t height, PixelFormat pixel_format, std::optional<glm::vec4> clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), ResourceUsage extra_usage = ResourceUsage::none) = 0;
        virtual ResourceHandlePair create_depth_target(const std::string& name, uint32_t width, uint32_t height, PixelFormat pixel_format, float clear_value = 1.0f) = 0;
        virtual void resize_texture(ResourceHandlePair& texture, const uint32_t width, const uint32_t height) = 0;
        virtual void update_buffer(const ResourceHandlePair& buffer, const uint32_t offset, const uint32_t n_bytes, const void* data) = 0;
        virtual void readback_buffer(const ResourceHandlePair& buffer, const uint32_t offset, const uint32_t n_bytes, void* destination) = 0;
        virtual void queue_unload_bindless_resource(ResourceHandlePair resource) = 0;
        virtual void use_resource(const ResourceHandle handle, const ResourceUsage usage = ResourceUsage::read) = 0;
        virtual void use_resources(const std::initializer_list<ResourceTransitionInfo>& resources) = 0;

        // Raytracing resources
        virtual ResourceHandlePair create_acceleration_structure(const std::string& name, const size_t size) = 0;
        virtual ResourceHandlePair create_blas(const std::string& name, const ResourceHandlePair& position_buffer, const ResourceHandlePair& index_buffer, const uint32_t vertex_count, const uint32_t index_count) = 0;
        virtual ResourceHandlePair create_tlas(const std::string& name, const std::vector<RaytracingInstance>& instances) = 0;
    };
};
