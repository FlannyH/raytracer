#include "renderer.h"
#include "scene.h"
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>

namespace gfx {
    // Initialisation and state
    Renderer::Renderer(int width, int height, bool debug_layer_enabled) {
        m_device = std::make_unique<Device>(width, height, debug_layer_enabled);
        m_pipeline_scene = m_device->create_raster_pipeline("assets/shaders/test.vs.hlsl", "assets/shaders/test.ps.hlsl");
        m_pipeline_final_blit = m_device->create_raster_pipeline("assets/shaders/fullscreen_quad.vs.hlsl", "assets/shaders/final_blit.ps.hlsl");
        m_color_target = m_device->create_render_target("Color framebuffer", 256, 192, PixelFormat::rgba_8).handle;
        m_depth_target = m_device->create_depth_target("Depth framebuffer", 256, 192, PixelFormat::depth_f32).handle;
    }

    // Common rendering
    bool Renderer::should_stay_open() {
        return m_device->should_stay_open();
    }

    void Renderer::set_full_screen(bool full_screen) {
        m_device->set_full_screen(full_screen);
    }

    void Renderer::begin_frame() {
        int x = 0;
        int y = 0;
        m_device->get_window_size(x, y);
        if (x < 8) x = 8;
        if (y < 8) y = 8;
        m_resolution.x = (float)x;
        m_resolution.y = (float)y;
        m_device->begin_frame();

        render_queue_scenes.clear();
    }

    void Renderer::end_frame() {
        // Render scene
        m_device->begin_raster_pass(m_pipeline_scene, RasterPassInfo{
            .color_target = m_color_target,
            .depth_target = m_depth_target,
            .clear_on_begin = true,
        });
        for (auto& scene_handle : render_queue_scenes) {
            m_device->draw_scene(scene_handle);
        }
        m_device->end_raster_pass();

        // Final blit
        m_device->begin_raster_pass(m_pipeline_final_blit, RasterPassInfo{
            .color_target = ResourceHandle::none(), // render to swapchain
            .clear_on_begin = false, // We're blitting to the entire buffer, no need to clear first
        });
        m_device->set_root_constants({
            m_color_target.as_u32(), // Texture to blit to screen
        });
        m_device->draw_vertices(6); // 2 triangles making up a quad
        m_device->end_frame();
    }

    // todo: maybe make separate camera struct that holds the transform, fov, near and far plane, and also caches its matrices?
    void Renderer::set_camera(Transform& transform) {
        const PacketCamera camera_matrices = {
            .view_matrix = transform.as_view_matrix(),
            .projection_matrix = glm::perspectiveFov(glm::radians(70.f), m_resolution.x, m_resolution.y, 0.1f, 1000.0f),
        };

        m_camera_matrices_offset = m_device->create_draw_packet(&camera_matrices, sizeof(camera_matrices));
    }

    void Renderer::draw_scene(ResourceHandlePair scene_handle) {
        render_queue_scenes.push_back(scene_handle.handle);
    }

    // Resource management
    ResourceHandlePair Renderer::load_scene_gltf(const std::string& path) {
        return m_device->create_scene_graph_from_gltf(path);
    }
}
