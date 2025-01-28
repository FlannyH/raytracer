#include "renderer.h"
#include "scene.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include "input.h"

namespace gfx {
    #define MAX_MATERIAL_COUNT 1024
    #define DRAW_PACKET_BUFFER_SIZE 102400
    #define GPU_BUFFER_PREFERRED_ALIGNMENT 64
    #define MAX_LIGHTS_DIRECTIONAL 32
    #define MAX_CUBEMAP_SH 128
    #define FOV (glm::radians(70.f))

    // Initialisation and state
    Renderer::Renderer(int width, int height, bool debug_layer_enabled, bool gpu_profiling_enabled) {
        m_device = std::make_unique<Device>(width, height, debug_layer_enabled, gpu_profiling_enabled);

        LOG(Debug, "Creating framebuffers");
        m_position_target = m_device->create_render_target("Position framebuffer", width, height, PixelFormat::rgba32_float, glm::vec4(0.0f, 0.0f, 9999999.0f, 0.0f), ResourceUsage::compute_write);
        m_color_target = m_device->create_render_target("Color framebuffer", width, height, PixelFormat::rgba16_float, glm::vec4(0.0f, 0.0f, 0.0f, -1.0f), ResourceUsage::compute_write);
        m_normal_target = m_device->create_render_target("Normal framebuffer", width, height, PixelFormat::rgba16_float, {}, ResourceUsage::compute_write);
        m_metallic_roughness_target = m_device->create_render_target("Metallic & roughness framebuffer", width, height, PixelFormat::rg8_unorm, {}, ResourceUsage::compute_write);
        m_emissive_target = m_device->create_render_target("Emissive framebuffer", width, height, PixelFormat::rg11_b10_float, {}, ResourceUsage::compute_write);
        m_shaded_target = m_device->load_texture("Shaded framebuffer", width, height, 1, nullptr, PixelFormat::rgba16_float, TextureType::tex_2d, ResourceUsage::compute_write);
        m_ssao_target = m_device->load_texture("SSAO framebuffer", width, height, 1, nullptr, PixelFormat::r8_unorm, TextureType::tex_2d, ResourceUsage::compute_write);
        m_accumulation_target = m_device->load_texture("Accumulation framebuffer", width, height, 1, nullptr, PixelFormat::rgba32_float, TextureType::tex_2d, ResourceUsage::compute_write);
        m_depth_target = m_device->create_depth_target("Depth framebuffer", width, height, PixelFormat::depth32_float);
        LOG(Debug, "Compiling shaders");
        m_pipeline_scene = m_device->create_raster_pipeline("Geometry pass"  ,"assets/shaders/rasterized/geo_pass.vs.hlsl", "assets/shaders/rasterized/geo_pass.ps.hlsl", {
            m_position_target,
            m_color_target,
            m_normal_target,
            m_metallic_roughness_target,
            m_emissive_target
        }, m_depth_target);
        m_pipeline_brdf = m_device->create_compute_pipeline("BRDF", "assets/shaders/rasterized/brdf.cs.hlsl");
        m_pipeline_tonemapping = m_device->create_compute_pipeline("Tonemapping", "assets/shaders/post/tonemapping.cs.hlsl");
        m_pipeline_final_blit = m_device->create_raster_pipeline("Final blit", "assets/shaders/fullscreen_tri.vs.hlsl", "assets/shaders/final_blit.ps.hlsl", {});
        m_pipeline_hdri_to_cubemap = m_device->create_compute_pipeline("HRDI to cubemap conversion" ,"assets/shaders/pre/hdri_to_cubemap.cs.hlsl");
        m_pipeline_cubemap_to_diffuse = m_device->create_compute_pipeline("Indirect diffuse spherical harmonics calculation" ,"assets/shaders/pre/cubemap_to_diffuse.cs.hlsl");
        m_pipeline_accumulate_sh_coeffs = m_device->create_compute_pipeline("Accumulate spherical harmonics coefficients", "assets/shaders/pre/accumulate_sh_coeffs.cs.hlsl");
        m_pipeline_compute_sh_matrices = m_device->create_compute_pipeline("Compute spherical harmonics matrices", "assets/shaders/pre/compute_sh_matrices.cs.hlsl");
        m_pipeline_prefilter_cubemap = m_device->create_compute_pipeline("Prefilter specular IBL cubemap" ,"assets/shaders/pre/prefilter_cubemap.cs.hlsl");
        m_pipeline_ibl_brdf_lut_gen = m_device->create_compute_pipeline("Generate IBL BRDF LUT", "assets/shaders/pre/ibl_brdf_lut_gen.cs.hlsl");
        m_pipeline_downsample = m_device->create_compute_pipeline("Downsample texture" ,"assets/shaders/pre/downsample.cs.hlsl");
        m_pipeline_ssao = m_device->create_compute_pipeline("SSAO" ,"assets/shaders/post/ssao.cs.hlsl");
        m_pipeline_pathtrace = m_device->create_compute_pipeline("Pathtrace" ,"assets/shaders/pathtraced/pathtrace.cs.hlsl");
        m_pipeline_reconstruct_normal_map = m_device->create_compute_pipeline("Reconstruct normal map Z component" ,"assets/shaders/pre/reconstruct_normal_map.cs.hlsl");
        
        LOG(Debug, "Creating buffers");
        m_material_buffer = m_device->create_buffer("Material descriptions", MAX_MATERIAL_COUNT * sizeof(Material), nullptr, ResourceUsage::cpu_writable);
        m_lights_buffer = m_device->create_buffer("Lights buffer", 3 * sizeof(uint32_t) + MAX_LIGHTS_DIRECTIONAL * sizeof(LightDirectional), nullptr, ResourceUsage::cpu_writable);
        m_spherical_harmonics_buffer = m_device->create_buffer("Spherical harmonics coefficients buffer", MAX_CUBEMAP_SH * 3*sizeof(glm::mat4), nullptr, ResourceUsage::compute_write);

        // Create triple buffered draw packet buffer
        for (int i = 0; i < backbuffer_count; ++i) {
            m_draw_packets[i] = m_device->create_buffer("Draw Packets", DRAW_PACKET_BUFFER_SIZE, nullptr, ResourceUsage::cpu_writable);
        }

        LOG(Debug, "Precalculating IBL BRDF LUT");
        constexpr uint32_t ibl_brdf_resolution = 512;
        m_env_brdf_lut = m_device->load_texture(
            "IBL BRDF LUT", ibl_brdf_resolution, ibl_brdf_resolution, 1, nullptr, 
            PixelFormat::rg16_float, TextureType::tex_2d, ResourceUsage::compute_write
        );
        m_device->begin_compute_pass(m_pipeline_ibl_brdf_lut_gen, true);
        m_device->use_resource(m_env_brdf_lut, ResourceUsage::compute_write);
        m_device->set_compute_root_constants({
            m_env_brdf_lut.handle.as_u32_uav(),
            ibl_brdf_resolution
        });
        m_device->dispatch_threadgroups(ibl_brdf_resolution / 8, ibl_brdf_resolution / 8, 1);
        m_device->end_compute_pass();

        LOG(Info, "Renderer initialized (DirectX 12)");
    }

    Renderer::~Renderer() {
        // Mark all resources for unloading
        for (const auto [handle, resource] : m_resources) {
            m_device->queue_unload_bindless_resource(ResourceHandlePair{
                .handle = {.id = handle},
                .resource = resource
            });
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

        // Update materials
        if (m_should_update_material_buffer) {
            m_should_update_material_buffer = false;
            m_device->update_buffer(m_material_buffer, 0, m_materials.size() * sizeof(Material), m_materials.data());
        }
        m_draw_packet_cursor = 0;

        // Begin frame handles swapchain resizes, which makes sure all the GPU operations finish first
        // This means we can be sure that resizing the render target textures is safe
        m_device->begin_frame();

        render_queue_scenes.clear();
        m_lights_directional.clear();
    }

    void Renderer::end_frame() {
        // If the render resolution changes, update the render targets
        const glm::vec2 prev_render_resolution = m_render_resolution;
        m_render_resolution = m_resolution * resolution_scale;
        if (m_render_resolution != prev_render_resolution) {
            m_view_data.viewport_size = {
                tan(FOV * 0.5f) * (m_resolution.x / m_resolution.y), // viewport width
                tan(FOV * 0.5f), // viewport height
            };
            resize_texture(m_position_target, (uint32_t)m_render_resolution.x, (uint32_t)m_render_resolution.y);
            resize_texture(m_color_target, (uint32_t)m_render_resolution.x, (uint32_t)m_render_resolution.y);
            resize_texture(m_normal_target, (uint32_t)m_render_resolution.x, (uint32_t)m_render_resolution.y);
            resize_texture(m_metallic_roughness_target, (uint32_t)m_render_resolution.x, (uint32_t)m_render_resolution.y);
            resize_texture(m_emissive_target, (uint32_t)m_render_resolution.x, (uint32_t)m_render_resolution.y);
            resize_texture(m_shaded_target, (uint32_t)m_render_resolution.x, (uint32_t)m_render_resolution.y);
            resize_texture(m_depth_target, (uint32_t)m_render_resolution.x, (uint32_t)m_render_resolution.y);
            resize_texture(m_ssao_target, (uint32_t)m_render_resolution.x, (uint32_t)m_render_resolution.y);
            resize_texture(m_accumulation_target, (uint32_t)m_render_resolution.x, (uint32_t)m_render_resolution.y);
        }
        
        // Upload light info to the GPU
        const uint32_t light_count[3] = {
            (uint32_t)m_lights_directional.size(),
            0, // todo: point lights
            0, // todo: spot lights
        };
        m_device->update_buffer(m_lights_buffer, 0, sizeof(light_count), &light_count);
        if (m_lights_directional.empty() == false) {
            m_device->update_buffer(
                m_lights_buffer,
                sizeof(light_count),
                sizeof(m_lights_directional[0]) * (uint32_t)m_lights_directional.size(),
                m_lights_directional.data()
            );
        }

        static int mode = 1;
        if (input::key_held(input::Key::_1)) mode = 0;
        if (input::key_held(input::Key::_2)) mode = 1;
        switch (mode) {
        case 0: render_rasterized(); break;
        case 1: render_pathtraced(); break;
        }

        // Tonemapping
        m_device->begin_compute_pass(m_pipeline_tonemapping);
        m_device->use_resource(m_shaded_target, ResourceUsage::compute_write);
        m_device->set_compute_root_constants({
            m_shaded_target.handle.as_u32_uav(),
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
        m_device->use_resource(m_shaded_target, ResourceUsage::pixel_shader_read);
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

        m_view_data.rotation = transform.rotation;
        m_view_data.camera_world_position = transform.position;
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

    bool Renderer::supports(RendererFeature feature) {
        return m_device->supports(feature);
    }

    void Renderer::render_rasterized() {
        // Queue rendering scenes
        m_device->begin_raster_pass(m_pipeline_scene, RasterPassInfo{
            .color_targets = {
                m_position_target,
                m_color_target,
                m_normal_target,
                m_metallic_roughness_target,
                m_emissive_target,
            },
            .depth_target = m_depth_target,
            .clear_on_begin = true,
        });
        for (auto& scene : render_queue_scenes) {
            render_scene_raster(scene.handle);
        }
        m_device->end_raster_pass();

        // SSAO
        m_device->begin_compute_pass(m_pipeline_ssao);
        m_device->use_resources({
            { m_position_target, ResourceUsage::non_pixel_shader_read },
            { m_normal_target, ResourceUsage::non_pixel_shader_read },
            { m_ssao_target, ResourceUsage::compute_write },
            { m_draw_packets[m_device->frame_index() % backbuffer_count], ResourceUsage::non_pixel_shader_read },
        });
        m_device->set_compute_root_constants({
            64, // n_samples
            to_fixed_16_16(0.0065f), // radius
            to_fixed_16_16(0.003f), // bias
            to_fixed_16_16(1.0f), // strength
            (uint32_t)m_device->frame_index(),
            m_position_target.handle.as_u32(),
            m_normal_target.handle.as_u32(),
            m_ssao_target.handle.as_u32_uav(),
            m_draw_packets[m_device->frame_index() % backbuffer_count].handle.as_u32(),
            m_camera_matrices_offset,
        });
        m_device->dispatch_threadgroups( // threadgroup size is 8x8
            (uint32_t)(m_render_resolution.x / 8.0f),
            (uint32_t)(m_render_resolution.y / 8.0f),
            1
        );
        m_device->end_compute_pass();

        // BRDF
        const uint32_t view_data_offset = create_draw_packet(&m_view_data, sizeof(m_view_data));
        m_device->begin_compute_pass(m_pipeline_brdf);
        m_device->use_resources({
            { m_shaded_target, ResourceUsage::compute_write },
            { m_position_target, ResourceUsage::non_pixel_shader_read },
            { m_color_target, ResourceUsage::non_pixel_shader_read },
            { m_normal_target, ResourceUsage::non_pixel_shader_read },
            { m_metallic_roughness_target, ResourceUsage::non_pixel_shader_read },
            { m_emissive_target, ResourceUsage::non_pixel_shader_read },
            { m_ssao_target, ResourceUsage::non_pixel_shader_read },
            { m_lights_buffer, ResourceUsage::non_pixel_shader_read },
            { m_spherical_harmonics_buffer, ResourceUsage::non_pixel_shader_read },
            { m_curr_sky_cube.sky, ResourceUsage::non_pixel_shader_read },
            { m_curr_sky_cube.ibl, ResourceUsage::non_pixel_shader_read },
            { m_env_brdf_lut, ResourceUsage::non_pixel_shader_read },
            { m_draw_packets[m_device->frame_index() % backbuffer_count], ResourceUsage::non_pixel_shader_read }
        });
        m_device->set_compute_root_constants({
            m_shaded_target.handle.as_u32_uav(),
            m_position_target.handle.as_u32(),
            m_color_target.handle.as_u32(),
            m_normal_target.handle.as_u32(),
            m_metallic_roughness_target.handle.as_u32(),
            m_emissive_target.handle.as_u32(),
            m_ssao_target.handle.as_u32(),
            m_lights_buffer.handle.as_u32(),
            m_spherical_harmonics_buffer.handle.as_u32(),
            m_curr_sky_cube.sky.handle.as_u32(),
            m_curr_sky_cube.ibl.handle.as_u32(),
            m_curr_sky_cube.offset_diffuse_sh,
            (uint32_t)(m_curr_sky_cube.ibl.resource != nullptr ? m_curr_sky_cube.ibl.resource->subresource_handles.size() : 0),
            m_draw_packets[m_device->frame_index() % backbuffer_count].handle.as_u32(),
            view_data_offset,
            m_env_brdf_lut.handle.as_u32()
        });
        m_device->dispatch_threadgroups( // threadgroup size is 8x8
            (uint32_t)(m_render_resolution.x / 8.0f),
            (uint32_t)(m_render_resolution.y / 8.0f),
            1
        );
        m_device->end_compute_pass();
    }

    void Renderer::render_pathtraced() {
        const uint32_t view_data_offset = create_draw_packet(&m_view_data, sizeof(m_view_data));
        
        // todo: unhardcode
        const auto& scene_handle = render_queue_scenes.begin();
        SceneNode* scene = render_queue_scenes.begin()->resource->expect_scene().root;

        m_device->begin_compute_pass(m_pipeline_pathtrace);
        m_device->use_resources({
            { scene->expect_root().tlas, ResourceUsage::acceleration_structure },
            { m_material_buffer, ResourceUsage::non_pixel_shader_read },
            { m_accumulation_target, ResourceUsage::compute_write },
            { m_shaded_target, ResourceUsage::compute_write },
            { m_curr_sky_cube.sky, ResourceUsage::non_pixel_shader_read },
            { m_draw_packets[m_device->frame_index() % backbuffer_count], ResourceUsage::non_pixel_shader_read }
        });
        m_device->set_compute_root_constants({
            (uint32_t)input::mouse_button(input::MouseButton::right), // reset accumulation buffer
            1, // enable anti aliasing
            4, // rays per pixel
            4, // bounces per ray
            scene->expect_root().tlas.handle.as_u32(),
            m_accumulation_target.handle.as_u32_uav(),
            m_shaded_target.handle.as_u32_uav(),
            m_curr_sky_cube.sky.handle.as_u32(),
            m_material_buffer.handle.as_u32(),
            m_draw_packets[m_device->frame_index() % backbuffer_count].handle.as_u32(),
            view_data_offset,
            (uint32_t)m_device->frame_index(),
        });
        m_device->dispatch_threadgroups( // threadgroup size is 8x8
            (uint32_t)(m_render_resolution.x / 8.0f),
            (uint32_t)(m_render_resolution.y / 8.0f),
            1
        );
        m_device->end_compute_pass();    
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
        auto texture = load_texture(path, width, height, 1, data, PixelFormat::rgba8_unorm, TextureType::tex_2d, ResourceUsage::compute_write, true); // todo: implement mipmapping and set this to true
        stbi_image_free(data);
        return texture;
    }

    ResourceHandlePair Renderer::load_texture(const std::string& name, uint32_t width, uint32_t height, uint32_t depth, void* data, PixelFormat pixel_format, TextureType type, ResourceUsage usage, bool allocate_mips) {
        ResourceHandlePair texture = m_device->load_texture(name, width, height, depth, data, pixel_format, type, usage, allocate_mips ? 999 : 1);
        m_resources[texture.handle.id] = texture.resource;
        return texture;
    }

    ResourceHandlePair Renderer::create_buffer(const std::string& name, size_t size, void* data, ResourceUsage usage) {
        ResourceHandlePair buffer = m_device->create_buffer(name, size, data, usage);
        m_resources[buffer.handle.id] = buffer.resource;
        return buffer;
    }

    ResourceHandlePair Renderer::create_blas(const std::string& name, const ResourceHandlePair& position_buffer, const ResourceHandlePair& index_buffer, const uint32_t vertex_count, const uint32_t index_count) {
        ResourceHandlePair blas = m_device->create_blas(name, position_buffer, index_buffer, vertex_count, index_count);
        m_resources[blas.handle.id] = blas.resource;
        return blas;
    }

    ResourceHandlePair Renderer::create_tlas(const std::string& name, const std::vector<RaytracingInstance>& instances) {
        ResourceHandlePair tlas = m_device->create_tlas(name, instances);
        m_resources[tlas.handle.id] = tlas.resource;
        return tlas;
    }

    void Renderer::resize_texture(ResourceHandlePair& texture, const uint32_t width, const uint32_t height) {
        uint32_t padded_width = width;
        uint32_t padded_height = height;
        add_and_align(padded_width, (uint32_t)0, (uint32_t)8);
        add_and_align(padded_height, (uint32_t)0, (uint32_t)8);
        m_device->resize_texture(texture, padded_width, padded_height);
    }

    void Renderer::generate_mipmaps(ResourceHandlePair& texture) {
            if (!texture.handle.is_loaded) return;

            // Downsample base texture to mip 1
            uint32_t target_width = texture.resource->expect_texture().width / 2;
            uint32_t target_height = texture.resource->expect_texture().height / 2;
            
            uint32_t target_depth = texture.resource->expect_texture().depth;
            if (target_depth > 1) {
                LOG(Warning, "Mip generation only supported for 2D textures for now!");
            }
            m_device->begin_compute_pass(m_pipeline_downsample, true);
            m_device->use_resources({
                { texture, ResourceUsage::non_pixel_shader_read, 0 },
                { texture, ResourceUsage::compute_write, 1 }
            });
            m_device->set_compute_root_constants({
                texture.handle.as_u32_uav(),
                texture.resource->subresource_handles[0].as_u32_uav(),
                target_width, target_height,
                4, // RGBA, so 4 components
                0, // only support 2D for now
            });
            m_device->dispatch_threadgroups((target_width + 7) / 8, (target_height + 7) / 8, 1);

            uint32_t i = 0;
            while (true) {
                target_width /= 2;
                target_height /= 2;
                ++i;
                if (target_width <= 1 && target_height <= 1) break;
                if (i >= texture.resource->subresource_handles.size()) break;

                m_device->use_resources({
                    { texture, ResourceUsage::non_pixel_shader_read, i },
                    { texture, ResourceUsage::compute_write, i+1 }
                });
                m_device->set_compute_root_constants({
                    texture.resource->subresource_handles[i-1].as_u32_uav(),
                    texture.resource->subresource_handles[i].as_u32_uav(),
                    target_width, target_height,
                    4, // RGBA, so 4 components
                    0, // only support 2D for now
                });
                m_device->dispatch_threadgroups((target_width + 7) / 8, (target_height + 7) / 8, 1);
            }

            m_device->end_compute_pass();
    }

    void Renderer::reconstruct_normal_map(ResourceHandlePair& texture) {
        const uint32_t width = texture.resource->expect_texture().width;
        const uint32_t height = texture.resource->expect_texture().height;
        m_device->begin_compute_pass(m_pipeline_reconstruct_normal_map, true);
        m_device->use_resources({
            { texture, ResourceUsage::compute_write }
        });
        m_device->set_compute_root_constants({
            texture.handle.as_u32_uav(),
            width, height
        });
        m_device->dispatch_threadgroups((width + 7 / 8), (height + 7 / 8), 1);
        m_device->end_compute_pass();
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
        const auto resource = std::make_shared<Resource>(ResourceType::scene);
        resource->expect_scene().root = gfx::create_scene_graph_from_gltf(*this, path);

        ResourceHandle handle = allocate_non_gpu_resource_handle(ResourceType::scene);
        m_resources[handle.id] = resource;

        return ResourceHandlePair{ handle, resource };
    }

    Cubemap Renderer::load_environment_map(const std::string& path, const int sky_res, const int ibl_res, const float quality) {
        LOG(Debug, "Loading environment map \"%s\" at sky resolution %ix%ix6, and IBL resolution %ix%ix6", path.c_str(), sky_res, sky_res, ibl_res, ibl_res);
        if (sky_res < 2) {
            LOG(Error, "Sky resolution must be at least 2x2");
            return {};
        }
        if (sky_res < ibl_res / 2) {
            LOG(Warning, "Sky resolution (%ix%ix6) is less than half of the IBL resolution (%ix%ix6), resulting in poorer specular quality", sky_res, sky_res, ibl_res, ibl_res);
        }

        // Load HDRI from file
        stbi__vertically_flip_on_load = 0;
        int width, height, channels;
        glm::vec4* data = (glm::vec4*)stbi_loadf(path.c_str(), &width, &height, &channels, 4);

        if (!data) {
            LOG(Error, "Failed to load environment map \"%s\" - does the file exist?", path.c_str());
            return {};
        }

        std::vector<glm::vec4> cubemap_faces((size_t)(sky_res * sky_res * 6));

        // Convert HDRI to cubemap
        auto hdri = m_device->load_texture(path + "::(source hdri)", width, height, 1, data, PixelFormat::rgba32_float, TextureType::tex_2d);
        auto sky = m_device->load_texture(path + "::(sky cubemap)", sky_res, sky_res, 6, nullptr, PixelFormat::rgba32_float, TextureType::tex_cube, ResourceUsage::compute_write);
        auto ibl = m_device->load_texture(path + "::(specular ibl)", ibl_res, ibl_res, 6, nullptr, PixelFormat::rgba32_float, TextureType::tex_cube, ResourceUsage::compute_write, 9, 8);
        auto scratch = m_device->load_texture(path + "::(scratch buffer)", sky_res / 2, sky_res/ 2, 6, nullptr, PixelFormat::rgba32_float, TextureType::tex_cube, ResourceUsage::compute_write, 9, 8);
        m_device->begin_compute_pass(m_pipeline_hdri_to_cubemap, true);

        // Sky
        m_device->use_resources({
            { hdri, ResourceUsage::non_pixel_shader_read },
            { sky, ResourceUsage::compute_write }
        });
        m_device->set_compute_root_constants({
           hdri.handle.as_u32(),
           sky.handle.as_u32_uav(),
        });
        m_device->dispatch_threadgroups( // threadgroup size is 8x8x1
           (uint32_t)(sky_res / 8),
           (uint32_t)(sky_res / 8),
           6 // faces
        );

        // IBL
        m_device->use_resources({
            { hdri, ResourceUsage::non_pixel_shader_read },
            { ibl, ResourceUsage::compute_write }
        });
        m_device->set_compute_root_constants({
           hdri.handle.as_u32(),
           ibl.handle.as_u32_uav(),
        });
        m_device->dispatch_threadgroups( // threadgroup size is 8x8x1
           (uint32_t)(ibl_res / 8),
           (uint32_t)(ibl_res / 8),
           6 // faces
        );

        m_device->end_compute_pass();

        // Compute mipmaps
        uint32_t res = sky_res / 2;
        uint32_t dest_mip = 0;
        m_device->begin_compute_pass(m_pipeline_downsample, true);

        // First take data from the sky map, compute the first mip, and store it in the scratch buffer
        m_device->use_resources({
            { sky, ResourceUsage::non_pixel_shader_read },
            { scratch, ResourceUsage::compute_write, 0 }
        });
        m_device->set_compute_root_constants({
           sky.handle.as_u32_uav(),
           scratch.handle.as_u32_uav(),
           res, res, 4,
           1
        });
        m_device->dispatch_threadgroups( // threadgroup size is 8x8x1
           (uint32_t)(res / 8),
           (uint32_t)(res / 8),
           6 // faces
        );

        // Then keep downsampling until we reach 1x1
        while (res > 8) {
            res >>= 1;
            m_device->use_resources({
                { scratch, ResourceUsage::non_pixel_shader_read, dest_mip },
                { scratch, ResourceUsage::compute_write, dest_mip + 1 }
            });
            m_device->set_compute_root_constants({
                (dest_mip == 0) 
                ?   scratch.handle.as_u32_uav()
                :   scratch.resource->subresource_handles[dest_mip - 1].as_u32_uav(),
                scratch.resource->subresource_handles[dest_mip].as_u32_uav(),
                res, res, 4, // 4 components
                1 // is_cubemap = true
            });
            m_device->dispatch_threadgroups( // threadgroup size is 8x8x1
            (uint32_t)((res + 7) / 8),
            (uint32_t)((res + 7) / 8),
            6 // faces
            );
            ++dest_mip;
        }
        m_device->end_compute_pass();

        // Convert cubemap to spherical harmonics
        auto coeff_buffer = m_device->create_buffer(
            "Spherical harmonics per-pixel coefficients buffer", 
            ibl_res * ibl_res * 6 * sizeof(glm::vec3) * 9, 
            nullptr, ResourceUsage::compute_write
        );
        m_device->begin_compute_pass(m_pipeline_cubemap_to_diffuse, true);
        m_device->use_resources({
            { ibl, ResourceUsage::non_pixel_shader_read },
            { coeff_buffer, ResourceUsage::compute_write }
        });
        m_device->set_compute_root_constants({
            ibl.handle.as_u32(),
            coeff_buffer.handle.as_u32_uav(),
            (uint32_t)ibl_res
        });
        m_device->dispatch_threadgroups( // threadgroup size is 8x8
            (uint32_t)(ibl_res / 8),
            (uint32_t)(ibl_res / 8),
            6 // faces
        );
        m_device->end_compute_pass();

        // Calculate the number of threadgroups needed for the reduction
        constexpr auto reduction_per_pass = 256;
        uint32_t n_items = (uint32_t)(ibl_res * ibl_res * 6);
        uint32_t n_threadgroups = (n_items + reduction_per_pass - 1) / reduction_per_pass;

        // Create the temporary buffers
        auto scratch_buffer1 = m_device->create_buffer(
            "Spherical harmonics compute scratch buffer 1", 
            n_threadgroups * sizeof(glm::vec3) * 9, 
            nullptr, ResourceUsage::compute_write
        );
        auto scratch_buffer2 = m_device->create_buffer(
            "Spherical harmonics compute scratch buffer 2", 
            n_threadgroups * sizeof(glm::vec3) * 9, 
            nullptr, ResourceUsage::compute_write
        );

        // Pass 1
        m_device->begin_compute_pass(m_pipeline_accumulate_sh_coeffs, true);
        m_device->use_resources({
            { coeff_buffer, ResourceUsage::non_pixel_shader_read },
            { scratch_buffer1, ResourceUsage::compute_write },
        });
        m_device->set_compute_root_constants({
            scratch_buffer1.handle.as_u32_uav(),
            coeff_buffer.handle.as_u32_uav(),
            n_items
        });
        m_device->dispatch_threadgroups(n_threadgroups, 1, 1);
        m_device->end_compute_pass();

        // Keep reducing until we have 1 entry
        while (true) {
            // Even passes
            n_items = n_threadgroups;
            if (n_items == 1) {
                // Create matrix from scratch_buffer1 from the final values
                LOG(Debug, "n_items = %3i,   calculate matrices and store them at offset %i", n_items, m_spherical_harmonics_buffer_cursor);
                m_device->begin_compute_pass(m_pipeline_compute_sh_matrices, true);
                m_device->use_resources({
                    { scratch_buffer1, ResourceUsage::non_pixel_shader_read },
                    { m_spherical_harmonics_buffer, ResourceUsage::compute_write },
                });
                m_device->set_compute_root_constants({
                    scratch_buffer1.handle.as_u32(),
                    m_spherical_harmonics_buffer.handle.as_u32_uav(),
                    m_spherical_harmonics_buffer_cursor,
                    (uint32_t)(ibl_res * ibl_res * 6)
                });
                m_device->dispatch_threadgroups(1, 1, 1);
                m_device->end_compute_pass();
                // m_spherical_harmonics_buffer_cursor += 192;
                break;
            }

            n_threadgroups = (n_items + reduction_per_pass - 1) / reduction_per_pass;
            LOG(Debug, "n_items = %3i,   n_threadgroups = %3i", n_items, n_threadgroups);
            m_device->begin_compute_pass(m_pipeline_accumulate_sh_coeffs, true);
            m_device->use_resources({
                { scratch_buffer1, ResourceUsage::non_pixel_shader_read },
                { scratch_buffer2, ResourceUsage::compute_write },
            });
            m_device->set_compute_root_constants({
                scratch_buffer2.handle.as_u32_uav(),
                scratch_buffer1.handle.as_u32(),
                n_items
            });
            m_device->dispatch_threadgroups(n_threadgroups, 1, 1);
            m_device->end_compute_pass();

            // Odd passes
            n_items = n_threadgroups;
            if (n_items == 1) {
                // Create matrix from scratch_buffer2 from the final values
                LOG(Debug, "n_items = %3i,   calculate matrices and store them at offset %i", n_items, m_spherical_harmonics_buffer_cursor);
                m_device->begin_compute_pass(m_pipeline_compute_sh_matrices, true);
                m_device->use_resources({
                    { scratch_buffer2, ResourceUsage::non_pixel_shader_read },
                    { m_spherical_harmonics_buffer, ResourceUsage::compute_write },
                });
                m_device->set_compute_root_constants({
                    scratch_buffer2.handle.as_u32(),
                    m_spherical_harmonics_buffer.handle.as_u32_uav(),
                    m_spherical_harmonics_buffer_cursor,
                    (uint32_t)(ibl_res * ibl_res * 6)
                });
                m_device->dispatch_threadgroups(1, 1, 1);
                m_device->end_compute_pass();
                // m_spherical_harmonics_buffer_cursor += 192;
                break;
            }

            n_threadgroups = (n_items + reduction_per_pass - 1) / reduction_per_pass;
            LOG(Debug, "n_items = %3i,   n_threadgroups = %3i", n_items, n_threadgroups);
            m_device->begin_compute_pass(m_pipeline_accumulate_sh_coeffs, true);
            m_device->use_resources({
                { scratch_buffer2, ResourceUsage::non_pixel_shader_read },
                { scratch_buffer1, ResourceUsage::compute_write },
            });
            m_device->set_compute_root_constants({
                scratch_buffer1.handle.as_u32_uav(),
                scratch_buffer2.handle.as_u32(),
                n_items
            });
            m_device->dispatch_threadgroups(n_threadgroups, 1, 1);
            m_device->end_compute_pass();
        }

        // Pre-filter specular maps
        uint32_t mip_res = ibl_res / 2;
        float roughness = 0.0f;
        m_device->begin_compute_pass(m_pipeline_prefilter_cubemap, true);
        const auto& mip_handles = ibl.resource->subresource_handles;
        const float roughness_step = 1.0f / ((float)mip_handles.size());
        for (uint32_t i = 0; i < mip_handles.size(); ++i) {
            roughness += roughness_step;
            // Roughness values of 1.0f cause a div by zero on the GPU, and values above 1.0 are wrong
            if (roughness >= 1.0f) roughness = 0.99f;

            m_device->use_resources({
                {scratch, ResourceUsage::non_pixel_shader_read, 0},
                {ibl, ResourceUsage::compute_write, i + 1}
            });
            m_device->set_compute_root_constants({
                scratch.handle.as_u32(),
                mip_handles[i].as_u32_uav(),
                (uint32_t)mip_res,
                (uint32_t)mip_res,
                to_fixed_16_16(powf(roughness, 1.5f)),
                to_fixed_16_16(powf(quality, 2.0f))
            });
            LOG(Debug, "mip %2i, res: %4i: roughness: %.3f", i, mip_res, roughness);
            const uint32_t n_threadgroups = (mip_res + 7) / 8;
            m_device->dispatch_threadgroups(n_threadgroups, n_threadgroups, 6);
            mip_res /= 2;
        }
        m_device->end_compute_pass();

        // We won't need the original image data anymore
        stbi_image_free(data);
        
        // Now upload this texture as a cubemap
        return {
            .sky = sky,
            .ibl = ibl,
            .offset_diffuse_sh = 0
        };
    }

    uint32_t Renderer::create_draw_packet(const void* data, uint32_t size_bytes) {
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

    void Renderer::traverse_scene_raster(SceneNode* node) {
        if (!node) return;

        if (node->type == SceneNodeType::mesh) {
            auto draw_packet = PacketDrawMesh{
                .model_transform = node->cached_global_transform,
                .position_offset = glm::vec4(node->position_offset, 0.0f),
                .position_scale = glm::vec4(node->position_scale, 0.0f),
                .vertex_buffer = node->expect_mesh().vertex_buffer,
            };
            auto n_vertices = m_resources[draw_packet.vertex_buffer.id]->expect_buffer().size / sizeof(VertexCompressed);
            auto draw_packet_offset = create_draw_packet(&draw_packet, sizeof(draw_packet));
            m_device->use_resources({
                { m_draw_packets[m_device->frame_index() % backbuffer_count], ResourceUsage::non_pixel_shader_read, },
                { m_material_buffer, ResourceUsage::non_pixel_shader_read },
            });
            m_device->set_graphics_root_constants({
                m_draw_packets[m_device->frame_index() % backbuffer_count].handle.as_u32(),
                (uint32_t)m_camera_matrices_offset,
                (uint32_t)draw_packet_offset,
                m_material_buffer.handle.as_u32()
                });
            m_device->draw_vertices((uint32_t)n_vertices);
        }
        else if (node->type == SceneNodeType::light) {
            m_lights_directional.push_back(LightDirectional{
                .color = node->expect_light().color,
                .intensity = node->expect_light().intensity,
                .direction = glm::normalize(glm::vec3(node->cached_global_transform * glm::vec4(0.0, 0.0, -1.0, 0.0)) * m_view_data.rotation),
            });
        }
        for (auto& node : node->children) {
            traverse_scene_raster(node.get());
        }
    }

    void Renderer::render_scene_raster(ResourceHandle scene_handle) {
        std::shared_ptr<Resource> resource = m_resources[scene_handle.id];
        SceneNode* scene = m_resources[scene_handle.id]->expect_scene().root;
        traverse_scene_raster(scene);
    }
}
