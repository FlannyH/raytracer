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

        // Resource management
        ResourceHandlePair load_scene_gltf(const std::string& path);

    private:
        std::unique_ptr<Device> m_device;
        ResourceHandle m_color_target = ResourceHandle::none();
        size_t m_camera_matrices_offset = 0;
        glm::vec2 m_resolution = { 0.0f, 0.0f };
        std::vector<ResourceHandle> render_queue_scenes;
        std::shared_ptr<Pipeline> m_pipeline_scene = nullptr;
    };
}
