#include "renderer.h"
#include "scene.h"
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>

namespace gfx {
    // Initialisation and state
    Renderer::Renderer(int width, int height, bool debug_layer_enabled) {
        m_device = std::make_unique<Device>(width, height, debug_layer_enabled);
        m_color_target = m_device->create_render_target("Color framebuffer", width, height, PixelFormat::rgba16_float).handle;
        m_normal_target = m_device->create_render_target("Normal framebuffer", width,height, PixelFormat::rg11_b10_float).handle;
        m_roughness_metallic_target = m_device->create_render_target("Roughness framebuffer", width, height, PixelFormat::rg8_unorm).handle;
        m_emissive_target = m_device->create_render_target("Emissive framebuffer", width, height, PixelFormat::rg11_b10_float).handle;
        m_shaded_target = m_device->create_render_target("Shaded framebuffer", width, height, PixelFormat::rgba16_float).handle;
        m_depth_target = m_device->create_depth_target("Depth framebuffer", width, height, PixelFormat::depth32_float).handle;
        m_pipeline_scene = m_device->create_raster_pipeline("assets/shaders/geo_pass.vs.hlsl", "assets/shaders/geo_pass.ps.hlsl", {
            m_color_target,
            m_normal_target,
            m_roughness_metallic_target,
            m_emissive_target
        }, m_depth_target);
        m_pipeline_brdf = m_device->create_compute_pipeline("assets/shaders/brdf.cs.hlsl");
        m_pipeline_final_blit = m_device->create_raster_pipeline("assets/shaders/fullscreen_tri.vs.hlsl", "assets/shaders/final_blit.ps.hlsl", {});
    }

    // Common rendering
    bool Renderer::should_stay_open() {
        return m_device->should_stay_open();
    }

    void Renderer::set_full_screen(bool full_screen) {
        m_device->set_full_screen(full_screen);
    }

    void Renderer::begin_frame() {
        // Fetch window content size
        int x = 0;
        int y = 0;
        m_device->get_window_size(x, y);

        // Enforce minimum framebuffer size
        if (x < 8) x = 8;
        if (y < 8) y = 8;
        m_resolution.x = (float)x;
        m_resolution.y = (float)y;

        // If the render resolution changes, update the render targets
        const glm::vec2 prev_render_resolution = m_render_resolution;
        m_render_resolution = m_resolution * resolution_scale;
        if (m_render_resolution != prev_render_resolution) {
            resize_texture(m_color_target, (uint32_t)m_render_resolution.x, (uint32_t)m_render_resolution.y);
            resize_texture(m_normal_target, (uint32_t)m_render_resolution.x, (uint32_t)m_render_resolution.y);
            resize_texture(m_roughness_metallic_target, (uint32_t)m_render_resolution.x, (uint32_t)m_render_resolution.y);
            resize_texture(m_emissive_target, (uint32_t)m_render_resolution.x, (uint32_t)m_render_resolution.y);
            resize_texture(m_shaded_target, (uint32_t)m_render_resolution.x, (uint32_t)m_render_resolution.y);
            resize_texture(m_depth_target, (uint32_t)m_render_resolution.x, (uint32_t)m_render_resolution.y);
        }


        m_device->begin_frame();

        render_queue_scenes.clear();
    }

    void Renderer::end_frame() {
        // Render scene
        m_device->begin_raster_pass(m_pipeline_scene, RasterPassInfo{
            .color_targets = {
                m_color_target,
                m_normal_target,
                m_roughness_metallic_target,
                m_emissive_target,
            },
            .depth_target = m_depth_target,
            .clear_on_begin = true,
        });
        for (auto& scene_handle : render_queue_scenes) {
            m_device->draw_scene(scene_handle);
        }
        m_device->end_raster_pass();

        // BRDF
        m_device->begin_compute_pass(m_pipeline_brdf);
        m_device->set_compute_root_constants({
            m_shaded_target.as_u32(),
            m_color_target.as_u32(),
            m_normal_target.as_u32(),
            m_roughness_metallic_target.as_u32(),
            m_emissive_target.as_u32()
        });
        m_device->dispatch_threadgroups( // threadgroup size is 8x8
            (uint32_t)(m_render_resolution.x / 8.0f),
            (uint32_t)(m_render_resolution.y / 8.0f),
            1
        );
        m_device->end_compute_pass();

        // Final blit
        m_device->begin_raster_pass(m_pipeline_final_blit, RasterPassInfo{
            .color_targets = {}, // render to swapchain
            .clear_on_begin = false, // We're blitting to the entire buffer, no need to clear first
        });
        m_device->set_graphics_root_constants({
            m_shaded_target.as_u32(), // Texture to blit to screen
        });
        m_device->draw_vertices(3); // 2 triangles making up a quad
        m_device->end_raster_pass();

        // API specific end frame
        m_device->end_frame();
    }

    // todo: maybe make separate camera struct that holds the transform, fov, near and far plane, and also caches its matrices?
    void Renderer::set_camera(Transform& transform) {
        const PacketCamera camera_matrices = {
            .view_matrix = transform.as_view_matrix(),
            .projection_matrix = glm::perspectiveFov(glm::radians(70.f), m_resolution.x, m_resolution.y, 0.0001f, 1000.0f),
        };

        m_camera_matrices_offset = m_device->create_draw_packet(&camera_matrices, sizeof(camera_matrices));
    }

    void Renderer::draw_scene(ResourceHandlePair scene_handle) {
        render_queue_scenes.push_back(scene_handle.handle);
    }

    void Renderer::set_resolution_scale(glm::vec2 scale) {
        resolution_scale = scale;
    }

    void Renderer::resize_texture(ResourceHandle& texture, const uint32_t width, const uint32_t height) {
        uint32_t padded_width = width;
        uint32_t padded_height = height;
        add_and_align(padded_width, (uint32_t)0, (uint32_t)8);
        add_and_align(padded_height, (uint32_t)0, (uint32_t)8);
        m_device->resize_texture(texture, padded_width, padded_height);
    }

    // Resource management
    ResourceHandlePair Renderer::load_scene_gltf(const std::string& path) {
        return m_device->create_scene_graph_from_gltf(path);
    }
}
