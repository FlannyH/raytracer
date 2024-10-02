#include "renderer.h"
#include "scene.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>

namespace gfx {
    #define MAX_MATERIAL_COUNT 1024
    #define DRAW_PACKET_BUFFER_SIZE 102400
    #define GPU_BUFFER_PREFERRED_ALIGNMENT 64
    #define MAX_LIGHTS_DIRECTIONAL 32

    // Initialisation and state
    Renderer::Renderer(int width, int height, bool debug_layer_enabled) {
        m_device = std::make_unique<Device>(width, height, debug_layer_enabled);
        m_color_target = m_device->create_render_target("Color framebuffer", width, height, PixelFormat::rgba16_float);
        m_normal_target = m_device->create_render_target("Normal framebuffer", width,height, PixelFormat::rg11_b10_float);
        m_roughness_metallic_target = m_device->create_render_target("Roughness framebuffer", width, height, PixelFormat::rg8_unorm);
        m_emissive_target = m_device->create_render_target("Emissive framebuffer", width, height, PixelFormat::rg11_b10_float);
        m_shaded_target = m_device->create_render_target("Shaded framebuffer", width, height, PixelFormat::rgba16_float);
        m_depth_target = m_device->create_depth_target("Depth framebuffer", width, height, PixelFormat::depth32_float);
        m_pipeline_scene = m_device->create_raster_pipeline("assets/shaders/geo_pass.vs.hlsl", "assets/shaders/geo_pass.ps.hlsl", {
            m_color_target,
            m_normal_target,
            m_roughness_metallic_target,
            m_emissive_target
        }, m_depth_target);
        m_pipeline_brdf = m_device->create_compute_pipeline("assets/shaders/brdf.cs.hlsl");
        m_pipeline_tonemapping = m_device->create_compute_pipeline("assets/shaders/tonemapping.cs.hlsl");
        m_pipeline_final_blit = m_device->create_raster_pipeline("assets/shaders/fullscreen_tri.vs.hlsl", "assets/shaders/final_blit.ps.hlsl", {});
        m_material_buffer = m_device->create_buffer("Material descriptions", MAX_MATERIAL_COUNT * sizeof(Material), nullptr);
        m_lights_buffer = m_device->create_buffer("Lights buffer", 3 * sizeof(uint32_t) + MAX_LIGHTS_DIRECTIONAL * sizeof(LightDirectional), nullptr);

        // Create triple buffered draw packet buffer
        for (int i = 0; i < backbuffer_count; ++i) {
            m_draw_packets[i] = m_device->create_buffer("Draw Packets", DRAW_PACKET_BUFFER_SIZE, nullptr);
        }
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

        if (m_should_update_material_buffer) {
            m_should_update_material_buffer = false;
            char* mapped_material_buffer = nullptr;
            const D3D12_RANGE read_range = { 0, 0 };
            validate(m_material_buffer.resource->handle->Map(0, &read_range, (void**)&mapped_material_buffer));
            memcpy(mapped_material_buffer, m_materials.data(), m_materials.size() * sizeof(Material));
            m_material_buffer.resource->handle->Unmap(0, &read_range);
        }
        m_draw_packet_cursor = 0;

        m_device->begin_frame();

        render_queue_scenes.clear();
        m_lights_directional.clear();
    }

    void Renderer::end_frame() {
        // Queue rendering scenes
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
        for (auto& scene : render_queue_scenes) {
            render_scene(scene.handle);
        }
        m_device->end_raster_pass();

        // Upload light info to the GPU
        const uint32_t light_count[3] = {
            m_lights_directional.size(),
            0, // todo: point lights
            0, // todo: spot lights
        };
        m_device->update_buffer(m_lights_buffer, 0, sizeof(light_count), &light_count);
        m_device->update_buffer(
            m_lights_buffer,
            sizeof(light_count),
            sizeof(m_lights_directional[0]) * m_lights_directional.size(),
            m_lights_directional.data()
        );

        // BRDF
        m_device->begin_compute_pass(m_pipeline_brdf);
        m_device->set_compute_root_constants({
            m_shaded_target.handle.as_u32(),
            m_color_target.handle.as_u32(),
            m_normal_target.handle.as_u32(),
            m_roughness_metallic_target.handle.as_u32(),
            m_emissive_target.handle.as_u32(),
            m_lights_buffer.handle.as_u32(),
        });
        m_device->dispatch_threadgroups( // threadgroup size is 8x8
            (uint32_t)(m_render_resolution.x / 8.0f),
            (uint32_t)(m_render_resolution.y / 8.0f),
            1
        );
        m_device->end_compute_pass();

        // Tonemapping
        m_device->begin_compute_pass(m_pipeline_tonemapping);
        m_device->set_compute_root_constants({
            m_shaded_target.handle.as_u32(),
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
            m_shaded_target.handle.as_u32(), // Texture to blit to screen
        });
        m_device->draw_vertices(3); // Triangle covering entire screen
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

        m_camera_matrices_offset = create_draw_packet(&camera_matrices, sizeof(camera_matrices));
    }

    void Renderer::draw_scene(ResourceHandlePair scene_handle) {
        render_queue_scenes.push_back(scene_handle);
    }

    void Renderer::set_resolution_scale(glm::vec2 scale) {
        resolution_scale = scale;
    }

    void Renderer::unload_resource(ResourceHandlePair& resource) {
        // Hand it over to the device, so it can unload the corresponding GPU resources once the GPU is done using them
        m_device->queue_unload_bindless_resource(resource);
        
        // Since the device now owns the resource, we can get rid of it here
        m_resources.erase(resource.handle.id);
    }

    ResourceHandlePair Renderer::load_texture(const std::string& path) {
        stbi__vertically_flip_on_load = 0;
        int width, height, channels;
        uint8_t* data = stbi_load(path.c_str(), &width, &height, &channels, 4);

        return load_texture(path, width, height, data, PixelFormat::rgba8_unorm);
    }

    ResourceHandlePair Renderer::load_texture(const std::string& name, uint32_t width, uint32_t height, void* data, PixelFormat pixel_format) {
        ResourceHandlePair texture = m_device->load_texture(name, width, height, data, pixel_format);
        m_resources[texture.handle.id] = texture.resource;
        return texture;
    }

    ResourceHandlePair Renderer::create_buffer(const std::string& name, size_t size, void* data) {
        ResourceHandlePair buffer = m_device->create_buffer(name, size, data);
        m_resources[buffer.handle.id] = buffer.resource;
        return buffer;
    }

    void Renderer::resize_texture(ResourceHandlePair& texture, const uint32_t width, const uint32_t height) {
        uint32_t padded_width = width;
        uint32_t padded_height = height;
        add_and_align(padded_width, (uint32_t)0, (uint32_t)8);
        add_and_align(padded_height, (uint32_t)0, (uint32_t)8);
        m_device->resize_texture(texture, padded_width, padded_height);
    }

    std::pair<int, Material*> Renderer::allocate_material_slot() {
        m_should_update_material_buffer = true;

        // Reuse old unused material slot if one is available
        if (!m_material_indices_to_reuse.empty()) {
            const std::pair<int, Material*> return_value = {
                m_material_indices_to_reuse[0],
                &m_materials[m_material_indices_to_reuse[0]]
            };
            m_material_indices_to_reuse.pop_back();
        }

        // Otherwise create a new slot
        const size_t slot_id = m_materials.size();
        m_materials.push_back({});
        return {
            slot_id,
            &m_materials[slot_id]
        };
    }

    ResourceHandle Renderer::allocate_non_gpu_resource_handle(ResourceType type) {
        ResourceHandle handle {
            .id = m_non_gpu_resource_handle_cursor | (1 << 26),
            .type = (uint32_t)type
        };

        if (!m_non_gpu_resource_handles_to_reuse.empty()) {
            handle.id = m_non_gpu_resource_handles_to_reuse.back() | (1 << 26);
            m_non_gpu_resource_handles_to_reuse.pop_back();
        }
        else {
            m_non_gpu_resource_handle_cursor++;
        }

        return handle;
    }

    ResourceHandlePair Renderer::load_scene_gltf(const std::string& path) {
        const auto resource = std::make_shared<Resource>();
        resource->type = ResourceType::scene;
        resource->scene_resource.root = gfx::create_scene_graph_from_gltf(*this, path);

        ResourceHandle handle = allocate_non_gpu_resource_handle(ResourceType::scene);
        m_resources[handle.id] = resource;

        return ResourceHandlePair{ handle, resource };
    }

    size_t Renderer::create_draw_packet(const void* data, size_t size_bytes) {
        // Allocate data in the draw packets buffer
        assert(((m_draw_packet_cursor + size_bytes) < DRAW_PACKET_BUFFER_SIZE) && "Failed to allocate draw packet: buffer overflow!");

        const size_t start = m_draw_packet_cursor;
        m_device->update_buffer(
            m_draw_packets[m_device->frame_index() % backbuffer_count], 
            m_draw_packet_cursor,
            size_bytes,
            data
        );

        // Return the byte offset of that draw packet, and update the cursor to the next entry, wrapping at the end
        add_and_align(m_draw_packet_cursor, size_bytes, (size_t)GPU_BUFFER_PREFERRED_ALIGNMENT);
        return start;
    }

    void Renderer::traverse_scene(SceneNode* node) {
        if (!node) return;

        if (node->type == SceneNodeType::Mesh) {
            auto draw_packet = PacketDrawMesh{
                .model_transform = node->cached_global_transform,
                .position_offset = glm::vec4(node->position_offset, 0.0f),
                .position_scale = glm::vec4(node->position_scale, 0.0f),
                .vertex_buffer = node->mesh.vertex_buffer,
            };
            auto n_vertices = m_resources[draw_packet.vertex_buffer.id]->expect_buffer().size / sizeof(VertexCompressed);
            auto draw_packet_offset = create_draw_packet(&draw_packet, sizeof(draw_packet));
            m_device->set_graphics_root_constants({
                m_draw_packets[m_device->frame_index() % backbuffer_count].handle.as_u32(),
                (uint32_t)m_camera_matrices_offset,
                (uint32_t)draw_packet_offset,
                m_material_buffer.handle.as_u32()
                });
            m_device->draw_vertices((uint32_t)n_vertices);
        }
        else if (node->type == SceneNodeType::Light) {
            m_lights_directional.push_back(LightDirectional{
                .color = node->light.color,
                .intensity = node->light.intensity,
                .direction = glm::normalize(glm::vec3(node->cached_global_transform * glm::vec4(0.0, 0.0, -1.0, 0.0))),
            });
        }
        for (auto& node : node->children) {
            traverse_scene(node.get());
        }
    }

    void Renderer::render_scene(ResourceHandle scene_handle) {
        std::shared_ptr<Resource> resource = m_resources[scene_handle.id];
        SceneNode* scene = m_resources[scene_handle.id]->expect_scene().root;
        traverse_scene(scene);
    }
}
