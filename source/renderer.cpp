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
    #define MAX_CUBEMAP_SH 128
    #define FOV (glm::radians(70.f))

    // Initialisation and state
    Renderer::Renderer(int width, int height, bool debug_layer_enabled) {
        m_device = std::make_unique<Device>(width, height, debug_layer_enabled);
        m_color_target = m_device->create_render_target("Color framebuffer", width, height, PixelFormat::rgba16_float);
        m_normal_target = m_device->create_render_target("Normal framebuffer", width,height, PixelFormat::rgba16_float);
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
        m_material_buffer = m_device->create_buffer("Material descriptions", MAX_MATERIAL_COUNT * sizeof(Material), nullptr, true);
        m_lights_buffer = m_device->create_buffer("Lights buffer", 3 * sizeof(uint32_t) + MAX_LIGHTS_DIRECTIONAL * sizeof(LightDirectional), nullptr, true);
        m_spherical_harmonics_buffer = m_device->create_buffer("Spherical harmonics coefficients buffer", MAX_CUBEMAP_SH * sizeof(SphericalHarmonicsDiffuse), nullptr, true);

        // Create triple buffered draw packet buffer
        for (int i = 0; i < backbuffer_count; ++i) {
            m_draw_packets[i] = m_device->create_buffer("Draw Packets", DRAW_PACKET_BUFFER_SIZE, nullptr, true);
        }
    }

    Renderer::~Renderer() {
        // Mark all resources for unloading
        for (const auto [handle, resource] : m_resources) {
            m_device->queue_unload_bindless_resource(ResourceHandlePair{
                .handle = {.id = handle},
                .resource = resource
            });
        }

        // Destroy device, which will wait for the GPU to finish, then unload the above resources
        m_device.reset();
    }

    // Common rendering
    bool Renderer::should_stay_open() {
        return m_device->should_stay_open();
    }

    void Renderer::set_full_screen(bool full_screen) {
        m_device->set_full_screen(full_screen);
    }

    void Renderer::begin_frame() {
        // Set normal map clear color to the current viewport data
        m_color_target.resource->expect_texture().clear_color = {
            tan(FOV * 0.5f) * (m_resolution.x / m_resolution.y), // viewport width
            tan(FOV * 0.5f), // viewport height
            -1.0f, // viewport depth
            -1.0f, // if this value is negative, it clearly doesn't come from the geo pass, so it means nothing was rendered there
        };

        // Update materials
        if (m_should_update_material_buffer) {
            m_should_update_material_buffer = false;
            char* mapped_material_buffer = nullptr;
            const D3D12_RANGE read_range = { 0, 0 };
            validate(m_material_buffer.resource->handle->Map(0, &read_range, (void**)&mapped_material_buffer));
            memcpy(mapped_material_buffer, m_materials.data(), m_materials.size() * sizeof(Material));
            m_material_buffer.resource->handle->Unmap(0, &read_range);
        }
        m_draw_packet_cursor = 0;

        // Begin frame handles swapchain resizes, which makes sure all the GPU operations finish first
        // This means we can be sure that resizing the render target textures is safe
        m_device->begin_frame();

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
            (uint32_t)m_lights_directional.size(),
            0, // todo: point lights
            0, // todo: spot lights
        };
        m_device->update_buffer(m_lights_buffer, 0, sizeof(light_count), &light_count);
        m_device->update_buffer(
            m_lights_buffer,
            sizeof(light_count),
            sizeof(m_lights_directional[0]) * (uint32_t)m_lights_directional.size(),
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
            m_spherical_harmonics_buffer.handle.as_u32(),
            m_curr_sky_cube.base.handle.as_u32(),
            m_curr_sky_cube.offset_diffuse_sh
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

        m_normal_target.resource->expect_texture().clear_color = { transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w };
        m_camera_matrices_offset = create_draw_packet(&camera_matrices, sizeof(camera_matrices));
    }

    void Renderer::set_skybox(Cubemap& sky) {
        m_curr_sky_cube = sky;
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

    ResourceHandlePair Renderer::load_texture(const std::string& path, bool free_after_upload) {
        stbi__vertically_flip_on_load = 0;
        int width, height, channels;
        uint8_t* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
        auto texture = load_texture(path, width, height, 1, data, PixelFormat::rgba8_unorm, TextureType::tex_2d);
        stbi_image_free(data);
        return texture;
    }

    ResourceHandlePair Renderer::load_texture(const std::string& name, uint32_t width, uint32_t height, uint32_t depth, void* data, PixelFormat pixel_format, TextureType type) {
        ResourceHandlePair texture = m_device->load_texture(name, width, height, depth, data, pixel_format, type);
        m_resources[texture.handle.id] = texture.resource;
        return texture;
    }

    ResourceHandlePair Renderer::create_buffer(const std::string& name, size_t size, void* data, bool cpu_visible) {
        ResourceHandlePair buffer = m_device->create_buffer(name, size, data, cpu_visible);
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
            return return_value;
        }

        // Otherwise create a new slot
        const int slot_id = (int)m_materials.size();
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
        resource->expect_scene().root = gfx::create_scene_graph_from_gltf(*this, path);

        ResourceHandle handle = allocate_non_gpu_resource_handle(ResourceType::scene);
        m_resources[handle.id] = resource;

        return ResourceHandlePair{ handle, resource };
    }

    Cubemap Renderer::load_environment_map(const std::string& path, const int resolution) {
        // Load HDRI from file
        stbi__vertically_flip_on_load = 0;
        int width, height, channels;
        glm::vec4* data = (glm::vec4*)stbi_loadf(path.c_str(), &width, &height, &channels, 4);

        if (!data) {
            printf("[ERROR] Failed to load environment map \"%s\"\n", path.c_str());
            return {};
        }

        // todo: maybe do this on the gpu?
        // todo: enforce dx12 padding rules
#define DIFFUSE_IRRADIANCE_RESOLUTION 8
        std::vector<glm::vec4> cubemap_faces((size_t)(resolution * resolution * 6));
        std::vector<glm::vec4> ibl_diffuse_cubemap_faces((size_t)(DIFFUSE_IRRADIANCE_RESOLUTION * DIFFUSE_IRRADIANCE_RESOLUTION * 6));

        // Convert HDRI to cubemap
        for (int face = 0; face < 6; ++face){
            for (int dst_y = 0; dst_y < resolution; ++dst_y) {
                for (int dst_x = 0; dst_x < resolution; ++dst_x) {
                    // Get UV coordinates for this face
                    const float u = ((float)dst_x / (float)resolution) * 2.0f - 1.0f;
                    const float v = ((float)dst_y / (float)resolution) * 2.0f - 1.0f;

                    // Convert to vector and normalize it
                    glm::vec3 dir(0.0f);
                    switch (face) {
                    case 0: dir = glm::normalize(glm::vec3(1.0f, v, u));   break;
                    case 1: dir = glm::normalize(glm::vec3(-1.0f, v, -u));  break;
                    case 2: dir = glm::normalize(glm::vec3(u, -1.0f, -v));   break;
                    case 3: dir = glm::normalize(glm::vec3(u, 1.0f, v));   break;
                    case 4: dir = glm::normalize(glm::vec3(u, v, -1.0f));    break;
                    case 5: dir = glm::normalize(glm::vec3(-u, v, 1.0f));   break;
                    }

                    // Convert to spherical coordinates
                    const float spherical_u = atan2f(dir.z, dir.x) / (2.0f * glm::pi<float>()) + 0.5f;
                    const float spherical_v = asin(dir.y) / (glm::pi<float>())+0.5f;

                    // Fetch pixel from texture and store in cubemap
                    glm::vec4 pixel = data[(size_t)(spherical_v * (height-1)) * width + (size_t)(spherical_u * (width-1))];
                    pixel.r = glm::min(pixel.r, 2000.0f); // some of these HDRIs get very very bright
                    pixel.g = glm::min(pixel.g, 2000.0f); // to avoid blowing out the entire equation
                    pixel.b = glm::min(pixel.b, 2000.0f); // let's cap them to a reasonable limit
                    pixel.a = glm::min(pixel.a, 2000.0f);
                    cubemap_faces.at((size_t)(dst_x + (dst_y * resolution) + (face * resolution * resolution))) = pixel;

                    // Get normal vector for this destination pixel
                    glm::vec3 dst_normal = glm::normalize(dir);
                }
            }
        }

        // Pre-compute diffuse irradiance based on this paper: https://graphics.stanford.edu/papers/envmap/envmap.pdf
        // todo: put this in a buffer or texture on the gpu
        SphericalHarmonicsDiffuse sh{};

        for (int face = 0; face < 6; ++face) {
            for (int y = 0; y < resolution; ++y) {
                for (int x = 0; x < resolution; ++x) {
                    // Get UV coordinates for this face
                    const float u = (((float)x + 0.5f) / (float)resolution) * 2.0f - 1.0f;
                    const float v = (((float)y + 0.5f) / (float)resolution) * 2.0f - 1.0f;

                    // Convert to vector and normalize it
                    glm::vec3 dir(0.0f);
                    switch (face) {
                    case 0: dir = glm::vec3(1.0f, v, u);   break;
                    case 1: dir = glm::vec3(-1.0f, v, -u);  break;
                    case 2: dir = glm::vec3(u, -1.0f, -v);   break;
                    case 3: dir = glm::vec3(u, 1.0f, v);   break;
                    case 4: dir = glm::vec3(u, v, -1.0f);    break;
                    case 5: dir = glm::vec3(-u, v, 1.0f);   break;
                    }

                    // Get normal vector for this destination pixel
                    glm::vec3 normal = glm::normalize(dir);

                    // Compute spherical harmonics coefficients
                    const glm::vec3 sample = cubemap_faces.at((size_t)(x + (y * resolution) + (face * resolution * resolution)));
                    sh.l00 += sample * (0.282095f);
                    sh.l11 += sample * (0.488603f * normal.x);
                    sh.l10 += sample * (0.488603f * normal.z);
                    sh.l1_1 += sample * (0.488603f * normal.y);
                    sh.l21 += sample * (1.092548f * normal.x * normal.z);
                    sh.l2_1 += sample * (1.092548f * normal.y * normal.z);
                    sh.l2_2 += sample * (1.092548f * normal.x * normal.y);
                    sh.l20 += sample * (0.315392f * (3.0f * normal.z * normal.z - 1.0f));
                    sh.l22 += sample * (0.546274f * (normal.x * normal.x - normal.y * normal.y));
                }
            }
        }

        sh.l00 /= (float)(resolution * resolution * 6);
        sh.l11 /= (float)(resolution * resolution * 6);
        sh.l10 /= (float)(resolution * resolution * 6);
        sh.l1_1 /= (float)(resolution * resolution * 6);
        sh.l21 /= (float)(resolution * resolution * 6);
        sh.l2_1 /= (float)(resolution * resolution * 6);
        sh.l2_2 /= (float)(resolution * resolution * 6);
        sh.l20 /= (float)(resolution * resolution * 6);
        sh.l22 /= (float)(resolution * resolution * 6);

        // We won't need the original image data anymore
        stbi_image_free(data);

        const uint32_t offset = m_spherical_harmonics_buffer_cursor;
        m_device->update_buffer(m_spherical_harmonics_buffer, m_spherical_harmonics_buffer_cursor, sizeof(SphericalHarmonicsDiffuse), &sh);
        m_spherical_harmonics_buffer_cursor += sizeof(SphericalHarmonicsDiffuse);

        // Now upload this texture as a cubemap
        return {
            .base = load_texture(path + "::(base cubemap)", resolution, resolution, 6, cubemap_faces.data(), PixelFormat::rgba32_float, TextureType::tex_cube),
            .offset_diffuse_sh = offset
        };
    }

    uint32_t Renderer::create_draw_packet(const void* data, uint32_t size_bytes) {
        auto reason = m_device->device->GetDeviceRemovedReason();
        // Allocate data in the draw packets buffer
        assert(((m_draw_packet_cursor + size_bytes) < DRAW_PACKET_BUFFER_SIZE) && "Failed to allocate draw packet: buffer overflow!");

        const uint32_t start = m_draw_packet_cursor;
        m_device->update_buffer(
            m_draw_packets[m_device->frame_index() % backbuffer_count], 
            m_draw_packet_cursor,
            size_bytes,
            data
        );

        // Return the byte offset of that draw packet, and update the cursor to the next entry, wrapping at the end
        add_and_align(m_draw_packet_cursor, size_bytes, (uint32_t)GPU_BUFFER_PREFERRED_ALIGNMENT);
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
