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
        void resize_texture(ResourceHandle& texture, const uint32_t width, const uint32_t height);

        // Resource management
        ResourceHandlePair load_scene_gltf(const std::string& path);

    private:
        std::unique_ptr<Device> m_device;
        ResourceHandle m_color_target = ResourceHandle::none();
        ResourceHandle m_normal_target = ResourceHandle::none();
        ResourceHandle m_roughness_metallic_target = ResourceHandle::none();
        ResourceHandle m_emissive_target = ResourceHandle::none();
        ResourceHandle m_depth_target = ResourceHandle::none();
        glm::vec2 m_resolution = { 0.0f, 0.0f };
        glm::vec2 m_render_resolution = { 0.0f, 0.0f };
        glm::vec2 resolution_scale = { 1.0f, 1.0f };
        size_t m_camera_matrices_offset = 0;
        std::vector<ResourceHandle> render_queue_scenes;
        std::shared_ptr<Pipeline> m_pipeline_scene = nullptr;
        std::shared_ptr<Pipeline> m_pipeline_final_blit = nullptr;
    };
}
