#pragma once
#include <memory>
#include "device.h"

namespace gfx {
    class Renderer {
    public:
        // Initialisation and state
        Renderer(int width, int height, bool debug_layer_enabled);

        // Common rendering
        bool should_stay_open();
        void set_full_screen(bool full_screen);
        void begin_frame();
        void end_frame();
        void set_camera(Transform& transform);
        void draw_scene(ResourceHandlePair scene_handle);
        void set_resolution_scale(glm::vec2 scale);

        // Resource management
        void unload_resource(ResourceHandlePair& resource);
        ResourceHandlePair load_texture(const std::string& path); // Load a texture from a file
        ResourceHandlePair load_texture(const std::string& name, uint32_t width, uint32_t height, void* data, PixelFormat pixel_format); // Load a texture from memory
        ResourceHandlePair create_buffer(const std::string& name, size_t size, void* data);
        ResourceHandlePair load_scene_gltf(const std::string& path);
        void resize_texture(ResourceHandlePair& texture, const uint32_t width, const uint32_t height);

        friend SceneNode* create_scene_graph_from_gltf(Renderer& renderer, const std::string& path);

    private:
        void traverse_scene(SceneNode* node);
        void render_scene(ResourceHandle scene_handle);
        std::pair<int, Material*> allocate_material_slot();
        ResourceHandle allocate_non_gpu_resource_handle(ResourceType type);
        size_t create_draw_packet(const void* data, const size_t size_bytes); // Returns the byte offset into the `m_draw_packets` buffer where this new draw packet was allocated

        std::unique_ptr<Device> m_device;
        std::unordered_map<uint32_t, std::shared_ptr<Resource>> m_resources; // Maps linking resource IDs and actual resource data
        std::unordered_map<uint32_t, bool> m_resource_residence; // Maps linking resource IDs to a boolean representing whether the resource is loaded on the GPU
        std::vector<uint32_t> m_non_gpu_resource_handles_to_reuse; // Non-GPU resources are resources that are useful on the CPU side, but don't directly correspond to a single GPU descriptor
        uint32_t m_non_gpu_resource_handle_cursor = 0;
        ResourceHandlePair m_color_target = { ResourceHandle::none(), nullptr };
        ResourceHandlePair m_normal_target = { ResourceHandle::none(), nullptr };
        ResourceHandlePair m_roughness_metallic_target = { ResourceHandle::none(), nullptr };
        ResourceHandlePair m_emissive_target = { ResourceHandle::none(), nullptr };
        ResourceHandlePair m_shaded_target = { ResourceHandle::none(), nullptr };
        ResourceHandlePair m_depth_target = { ResourceHandle::none(), nullptr };
        glm::vec2 m_resolution = { 0.0f, 0.0f };
        glm::vec2 m_render_resolution = { 0.0f, 0.0f };
        glm::vec2 resolution_scale = { 1.0f, 1.0f };
        std::vector<ResourceHandlePair> render_queue_scenes;
        std::shared_ptr<Pipeline> m_pipeline_scene = nullptr;
        std::shared_ptr<Pipeline> m_pipeline_brdf = nullptr;
        std::shared_ptr<Pipeline> m_pipeline_final_blit = nullptr;
        std::vector<int> m_material_indices_to_reuse;
        std::vector<Material> m_materials; // Should be uploaded to the GPU after modifying
        ResourceHandlePair m_material_buffer{}; // Buffer that contains all currently loaded materials
        bool m_should_update_material_buffer = false;
        ResourceHandlePair m_draw_packets[backbuffer_count]; // Scratch buffer that is used to send draw info to the shader pipelines
        size_t m_draw_packet_cursor = 0; // Current allocation offset into the draw packet buffer
        size_t m_camera_matrices_offset = 0; // Where the camera matrices for this frame are stored
    };
}
