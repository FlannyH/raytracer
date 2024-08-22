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
        void draw_scene(ResourceHandle scene_handle);

        // Resource management
        ResourceHandle load_scene_gltf(const std::string& path);

    private:
        std::unique_ptr<Device> m_device;
    };
}
