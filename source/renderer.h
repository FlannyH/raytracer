#pragma once
#include <memory>
#include "device.h"

namespace gfx {
    struct ViewData {
        glm::vec4 rotation{};
        glm::vec2 viewport_size{};
    };

    class Renderer {
    public:
        // Initialisation and state
        Renderer(int width, int height, bool debug_layer_enabled);
        ~Renderer();
        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        // Common rendering
        bool should_stay_open();
        void set_full_screen(bool full_screen);
        void begin_frame();
        void end_frame();
        void set_camera(Transform& transform);
        void set_skybox(Cubemap& sky);
        void draw_scene(ResourceHandlePair scene_handle);
        void set_resolution_scale(glm::vec2 scale);

        // Resource management
        void unload_resource(ResourceHandlePair& resource);
        ResourceHandlePair load_texture(const std::string& path, bool free_after_upload = true); // Load a texture from a file
        ResourceHandlePair load_texture(const std::string& name, uint32_t width, uint32_t height, uint32_t depth, void* data, PixelFormat pixel_format, TextureType type, ResourceUsage usage, bool generate_mips); // Load a texture from memory
        ResourceHandlePair create_buffer(const std::string& name, size_t size, void* data, bool cpu_visible);
        ResourceHandlePair load_scene_gltf(const std::string& path);
        Cubemap load_environment_map(const std::string& path, const int resolution = 1024);
        void resize_texture(ResourceHandlePair& texture, const uint32_t width, const uint32_t height);

        friend SceneNode* create_scene_graph_from_gltf(Renderer& renderer, const std::string& path);

    private:
        void traverse_scene(SceneNode* node);
        void render_scene(ResourceHandle scene_handle);
        std::pair<int, Material*> allocate_material_slot();
        ResourceHandle allocate_non_gpu_resource_handle(ResourceType type);
        uint32_t create_draw_packet(const void* data, uint32_t size_bytes); // Returns the byte offset into the `m_draw_packets` buffer where this new draw packet was allocated

        std::unique_ptr<Device> m_device;
        std::unordered_map<uint32_t, std::shared_ptr<Resource>> m_resources; // Maps linking resource IDs and actual resource data
        std::unordered_map<uint32_t, bool> m_resource_residence; // Maps linking resource IDs to a boolean representing whether the resource is loaded on the GPU
        std::vector<uint32_t> m_non_gpu_resource_handles_to_reuse; // Non-GPU resources are resources that are useful on the CPU side, but don't directly correspond to a single GPU descriptor
        uint32_t m_non_gpu_resource_handle_cursor = 0;
        ResourceHandlePair m_position_target = { ResourceHandle::none(), nullptr };
        ResourceHandlePair m_color_target = { ResourceHandle::none(), nullptr };
        ResourceHandlePair m_normal_target = { ResourceHandle::none(), nullptr };
        ResourceHandlePair m_metallic_roughness_target = { ResourceHandle::none(), nullptr };
        ResourceHandlePair m_emissive_target = { ResourceHandle::none(), nullptr };
        ResourceHandlePair m_shaded_target = { ResourceHandle::none(), nullptr };
        ResourceHandlePair m_depth_target = { ResourceHandle::none(), nullptr };
        ResourceHandlePair m_env_brdf_lut = { ResourceHandle::none(), nullptr };
        Cubemap m_curr_sky_cube = {};
        ResourceHandlePair m_spherical_harmonics_buffer = { ResourceHandle::none(), nullptr };
        uint32_t m_spherical_harmonics_buffer_cursor = 0;
        glm::vec2 m_resolution = { 0.0f, 0.0f };
        glm::vec2 m_render_resolution = { 0.0f, 0.0f };
        glm::vec2 resolution_scale = { 1.0f, 1.0f };
        std::vector<ResourceHandlePair> render_queue_scenes;
        std::shared_ptr<Pipeline> m_pipeline_scene = nullptr;
        std::shared_ptr<Pipeline> m_pipeline_brdf = nullptr;
        std::shared_ptr<Pipeline> m_pipeline_tonemapping = nullptr;
        std::shared_ptr<Pipeline> m_pipeline_final_blit = nullptr;
        std::shared_ptr<Pipeline> m_pipeline_hdri_to_cubemap = nullptr;
        std::shared_ptr<Pipeline> m_pipeline_cubemap_to_diffuse = nullptr;
        std::shared_ptr<Pipeline> m_pipeline_accumulate_sh_coeffs = nullptr;
        std::shared_ptr<Pipeline> m_pipeline_compute_sh_matrices = nullptr;
        std::shared_ptr<Pipeline> m_pipeline_prefilter_cubemap = nullptr;
        std::shared_ptr<Pipeline> m_pipeline_ibl_brdf_lut_gen = nullptr;
        std::vector<int> m_material_indices_to_reuse;
        std::vector<Material> m_materials; // Should be uploaded to the GPU after modifying
        ResourceHandlePair m_material_buffer{}; // Buffer that contains all currently loaded materials
        bool m_should_update_material_buffer = false;
        std::vector<LightDirectional> m_lights_directional; // All currently queued directional lights
        ResourceHandlePair m_lights_buffer{}; // Buffer that contains all queued lights for this frame
        ResourceHandlePair m_draw_packets[backbuffer_count]; // Scratch buffer that is used to send draw info to the shader pipelines
        uint32_t m_draw_packet_cursor = 0; // Current allocation offset into the draw packet buffer
        uint32_t m_camera_matrices_offset = 0; // Where the camera matrices for this frame are stored
        ViewData m_view_data{};
    };
}
