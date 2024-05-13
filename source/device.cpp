#define GLFW_EXPOSE_NATIVE_WIN32
#define STB_IMAGE_IMPLEMENTATION

#include "device.h"
#include <glfw/glfw3native.h>
#include <d3d12sdklayers.h>
#include <stb/stb_image.h>

#include <utility>

#include "buffer.h"
#include "command_buffer.h"
#include "swapchain.h"
#include "pipeline.h"
#include "descriptor_heap.h"
#include "command_queue.h"
#include "scene.h"
#include "input.h"

namespace gfx {
    #define N_DRAW_PACKETS 16384

    Device::Device(const int width, const int height, const bool debug_layer_enabled) {
        // Create window
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // use GLFW_NO_API, since we're not using OpenGL
        m_window_glfw = glfwCreateWindow(width, height, "Ray Tracer (DirectX 12)", nullptr, nullptr);
        window_hwnd = glfwGetWin32Window(m_window_glfw);

        UINT dxgi_factory_flags = 0;

        if (debug_layer_enabled) {
            // If we're in debug mode, create a debug layer for proper error tracking
            // Note: Errors will be printed in the Visual Studio output tab, and not in the console!
            dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
            validate(D3D12GetDebugInterface(IID_PPV_ARGS(&m_debug_layer)));
            validate(m_debug_layer->QueryInterface(IID_PPV_ARGS(&m_debug_layer)));
            m_debug_layer->EnableDebugLayer();
            m_debug_layer->SetEnableGPUBasedValidation(true);
            m_debug_layer->Release();
        }

        // Create factory
        validate(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&factory)));

        // Find adapter
        ComPtr<IDXGIAdapter1> adapter;
        UINT adapter_index = 0;
        while (factory->EnumAdapters1(adapter_index, &adapter) != DXGI_ERROR_NOT_FOUND) {
            DXGI_ADAPTER_DESC1 desc;
            validate(adapter->GetDesc1(&desc));

            // Ignore software renderer
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                continue;
            }

            // Does this adapter support Direct3D 12.0?
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)))) {
                // Yes it does! We use this one.
                break;
            }

            // It doesn't? Unfortunate, let's keep looking
            device = nullptr;
            adapter->Release();
            adapter_index++;
        }

        if (device == nullptr) {
            throw std::exception();
        }

        if (debug_layer_enabled) {
            validate(device->QueryInterface(m_device_debug.GetAddressOf()));
        }

        // Create descriptor heaps
        m_heap_rtv = std::make_shared<DescriptorHeap>(*this, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, backbuffer_count);
        m_heap_bindless = std::make_shared<DescriptorHeap>(*this, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1 / 2);

        // Create draw packet buffer
        DrawPacket* empty_draw_packets = new DrawPacket[N_DRAW_PACKETS];
        m_draw_packets = create_buffer("Draw Packets", sizeof(DrawPacket) * N_DRAW_PACKETS, empty_draw_packets);
        delete[] empty_draw_packets;
    }


    void Device::resize_window(const int width, const int height) const {
        glfwSetWindowSize(m_window_glfw, width, height);
    }

    void Device::get_window_size(int& width, int& height) const {
        glfwGetWindowSize(m_window_glfw, &width, &height);
    }

    void Device::init_context() {
        m_queue_gfx = std::make_shared<CommandQueue>(*this, CommandBufferType::graphics);
        m_upload_queue = std::make_shared<CommandQueue>(*this, CommandBufferType::graphics);
        m_swapchain = std::make_shared<Swapchain>(*this, *m_queue_gfx, *m_heap_rtv, m_framebuffer_format);
        input::init(m_window_glfw);
        get_window_size(m_width, m_height);
    }

    std::shared_ptr<Pipeline> Device::create_raster_pipeline(const std::string& vertex_shader, const std::string& pixel_shader) {
        return std::make_shared<Pipeline>(*this, vertex_shader, pixel_shader);
    }

    void Device::begin_frame() {
        static int prev_key = 0;
        int curr_key = glfwGetKey(m_window_glfw, GLFW_KEY_F11);
        if (curr_key == GLFW_PRESS && prev_key == GLFW_RELEASE) {
            set_full_screen(!m_is_fullscreen);
        }
        prev_key = curr_key;

        int width, height;
        get_window_size(width, height);
        if (m_width != width || m_height != height) {
            m_swapchain->resize(*this, m_queue_gfx, *m_heap_rtv, width, height, m_framebuffer_format);
            m_width = width;
            m_height = height;
        }
        m_upload_queue->execute();
        m_swapchain->next_framebuffer();
        m_queue_gfx->clean_up_old_command_buffers(m_swapchain->current_fence_completed_value());
    }

    void Device::end_frame() {
        m_swapchain->prepare_present(m_curr_pass_cmd);
        m_queue_gfx->execute();
        m_swapchain->synchronize(m_queue_gfx);
        m_swapchain->present();
        glfwPollEvents();
        glfwSwapBuffers(m_window_glfw);
    }

    // todo: maybe make separate camera struct that holds the transform, fov, near and far plane, and also caches its matrices?
    void Device::set_camera(Transform& transform) {
        DrawPacket packet;
        packet.camera_matrices = {
            .view_matrix = transform.as_view_matrix(),
            .projection_matrix = glm::perspectiveFov(glm::radians(70.f), (float)m_swapchain->width(), (float)m_swapchain->height(), 0.1f, 1000.0f),
        };
        m_camera_matrices_offset = create_draw_packet(packet);
    }

    void Device::begin_raster_pass(std::shared_ptr<Pipeline> pipeline, RasterPassInfo&& render_pass_info) {
        // Create command buffer for this pass
        m_curr_bound_pipeline = pipeline;
        m_curr_pass_cmd = m_queue_gfx->create_command_buffer(*this, pipeline.get(), m_swapchain->current_frame_index());

        // If the color target is the swapchain, prepare the swapchain for that
        if ((ResourceType)render_pass_info.color_target.type == ResourceType::none) {
            m_swapchain->prepare_render(m_curr_pass_cmd);
        }
    }

    void Device::end_raster_pass() {
        m_curr_bound_pipeline = nullptr;
    }

    void Device::draw_mesh(DrawPacket&& draw_info) {
        if (!m_curr_bound_pipeline) {
            printf("[ERROR] Attempt to record draw call without a pipeline set! Did you forget to call `begin_raster_pass()`?");
            return;
        }

        // Store draw packet
        size_t packet_offset = create_draw_packet(draw_info);

        // Get number of vertices from mesh
        auto vertex_buffer = m_resources[draw_info.draw_mesh.vertex_buffer.id];
        auto n_vertices = vertex_buffer->expect_buffer().size / sizeof(Vertex);

        // Get framebuffer and command buffer
        auto framebuffer = m_swapchain->curr_framebuffer();
        auto gfx_cmd = m_curr_pass_cmd->get();

        // Record draw call, binding all the resources in the process
        ID3D12DescriptorHeap* heaps[] = {
            m_heap_bindless->heap.Get(),
        };
        gfx_cmd->SetDescriptorHeaps(1, heaps);
        gfx_cmd->SetGraphicsRootSignature(m_curr_bound_pipeline->root_signature.Get());
        gfx_cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        gfx_cmd->SetPipelineState(m_curr_bound_pipeline->pipeline_state.Get());
        gfx_cmd->SetGraphicsRoot32BitConstant(0, m_draw_packets.handle.id, 0);
        gfx_cmd->SetGraphicsRoot32BitConstant(0, (uint32_t)m_camera_matrices_offset, 1);
        gfx_cmd->SetGraphicsRoot32BitConstant(0, (uint32_t)packet_offset, 2);
        gfx_cmd->DrawInstanced(n_vertices, 1, 0, 0);
    }

    void Device::traverse_scene(SceneNode* node) {
        if (node->type == SceneNodeType::Mesh) {
            draw_mesh(DrawPacket{
                .draw_mesh = {
                    .model_transform = node->cached_global_transform,
                    .vertex_buffer = node->mesh.vertex_buffer,
                    .texture = ResourceHandle::none(), // todo: add actual textures from the model here
                }
            });
        }
        for (auto& node : node->children) {
            traverse_scene(node.get());
        }
    }

    size_t Device::create_draw_packet(DrawPacket packet) {
        // Update the draw packet at the cursor position
        DrawPacket* mapped_buffer;
        const D3D12_RANGE write_range = { 
            m_draw_packet_cursor * sizeof(DrawPacket),
            (m_draw_packet_cursor + 1) * sizeof(DrawPacket)
        };
        validate(m_draw_packets.resource->handle->Map(0, &write_range, (void**)&mapped_buffer));
        memcpy(&mapped_buffer[m_draw_packet_cursor], &packet, sizeof(DrawPacket));
        m_draw_packets.resource->handle->Unmap(0, &write_range);

        // Return the byte offset of that draw packet, and update the cursor to the next entry, wrapping at the end
        const size_t byte_offset_into_buffer = m_draw_packet_cursor * sizeof(DrawPacket);
        m_draw_packet_cursor = (m_draw_packet_cursor + 1) % N_DRAW_PACKETS;
        return byte_offset_into_buffer;
    }

    void Device::draw_scene(ResourceHandle scene_handle) {
        std::shared_ptr<Resource> resource = m_resources[scene_handle.id];
        SceneNode* scene = m_resources[scene_handle.id]->expect_scene().root;
        traverse_scene(scene);
    }

    ResourceHandlePair Device::load_texture(const std::string& path) {
        stbi__vertically_flip_on_load = 1;
        int width, height, channels;
        uint8_t* data = stbi_load(path.c_str(), &width, &height, &channels, 4);

        return load_texture(path, width, height, data, PixelFormat::rgba_8);
    }

    ResourceHandlePair Device::load_texture(const std::string& name, uint32_t width, uint32_t height, void* data, PixelFormat pixel_format) {
        // Make texture resource
        const auto resource = std::make_shared<Resource>();
        *resource = {
            .type = ResourceType::texture,
            .texture_resource = {
                .data = static_cast<uint8_t*>(data),
                .width = width,
                .height = height,
                .pixel_format = DXGI_FORMAT_R8G8B8A8_UNORM,
            }
        };

        // Get the pixel format
        resource->expect_texture().pixel_format = pixel_format_to_dx12(pixel_format);

        // Create a d3d12 resource for the texture
        D3D12_RESOURCE_DESC resource_desc = {};
        resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resource_desc.Width = resource->expect_texture().width;
        resource_desc.Height = resource->expect_texture().height;
        resource_desc.DepthOrArraySize = 1;
        resource_desc.MipLevels = 1;
        resource_desc.Format = static_cast<DXGI_FORMAT>(resource->expect_texture().pixel_format);
        resource_desc.SampleDesc.Count = 1;
        resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
        resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        D3D12_HEAP_PROPERTIES heap_properties = {};
        heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;

        const auto upload_size = width * height * size_per_pixel(pixel_format);
        auto id = m_heap_bindless->alloc_descriptor(ResourceType::buffer);
        auto descriptor = m_heap_bindless->fetch_cpu_handle(id);
        validate(device->CreateCommittedResource(
            &heap_properties,
            D3D12_HEAP_FLAG_NONE,
            &resource_desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&resource->handle)
        ));
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
            .Format = resource_desc.Format,
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D = {
                .MostDetailedMip = 0,
                .MipLevels = 1,
                .PlaneSlice = 0,
                .ResourceMinLODClamp = 0.0f
            }
        };
        device->CreateShaderResourceView(resource->handle.Get(), &srv_desc, descriptor);

        // We need to copy the texture from an upload buffer
        const auto upload_buffer_id = create_buffer("Upload buffer", upload_size, data);

        const auto& upload_buffer = m_resources[static_cast<uint64_t>(upload_buffer_id.handle.id)];

        void* mapped_buffer;
        const D3D12_RANGE upload_range = { 0, upload_size };
        validate(upload_buffer->handle->Map(0, &upload_range, &mapped_buffer));
        memcpy(mapped_buffer, data, upload_size);
        upload_buffer->handle->Unmap(0, &upload_range);

        const auto texture_size_box = D3D12_BOX {
            .left = 0,
            .top = 0,
            .front = 0,
            .right = width,
            .bottom = height,
            .back = 1,
        };

        const auto texture_copy_source = D3D12_TEXTURE_COPY_LOCATION {
            .pResource = upload_buffer->handle.Get(),
            .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
            .PlacedFootprint = {
                .Offset = 0,
                .Footprint = {
                    .Format = static_cast<DXGI_FORMAT>(resource->expect_texture().pixel_format),
                    .Width = resource->expect_texture().width,
                    .Height = resource->expect_texture().height,
                    .Depth = 1,
                    .RowPitch = resource->expect_texture().width * static_cast<uint32_t>(size_per_pixel(pixel_format)),
                }
            }
        };

        const auto texture_copy_dest = D3D12_TEXTURE_COPY_LOCATION {
            .pResource = resource->handle.Get(),
            .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            .SubresourceIndex = 0,
        };

        const auto upload_command_buffer = m_upload_queue->create_command_buffer(*this, nullptr, ++m_upload_fence_value_when_done);
        const auto cmd = upload_command_buffer->get();
        cmd->CopyTextureRegion(&texture_copy_dest, 0, 0, 0, &texture_copy_source, &texture_size_box);
        validate(cmd->Close());

        id.is_loaded = true; // todo, only set this to true when the upload command buffer finished (when the fence value was reached)

        m_resource_name_map[name] = id;
        m_resources[id.id] = resource;

        return ResourceHandlePair{ id, resource };
    }

    ResourceHandlePair Device::load_mesh(const std::string& name, const uint64_t n_triangles, Triangle* tris) {
        return create_buffer(name, n_triangles * sizeof(Triangle), tris);
    }

    void debug_scene_graph_nodes(SceneNode* node, int depth = 0) {
        for (int i = 0; i < depth; ++i) printf("    ");
        switch (node->type) {
            case gfx::SceneNodeType::Empty:
                printf("Node: %s\n", node->name.c_str());
                break;
            case gfx::SceneNodeType::Mesh:
                printf("Mesh: (Vertex buffer: %i) %s\n", node->mesh.vertex_buffer.id, node->name.c_str());
                break;
            case gfx::SceneNodeType::Light:
                printf("Light: %s\n", node->name.c_str());
                break;
        }
        for (auto& child : node->children) {
            debug_scene_graph_nodes(child.get(), depth + 1);
        }
    }

    ResourceHandlePair Device::create_scene_graph_from_gltf(const std::string& path) {
        const auto resource = std::make_shared<Resource>();
        resource->type = ResourceType::scene;
        resource->scene_resource.root = gfx::create_scene_graph_from_gltf(*this, path);

        ResourceHandle handle = m_heap_bindless->alloc_descriptor(ResourceType::scene);
        m_resources[handle.id] = resource;

        return ResourceHandlePair{ handle, resource };
    }

    ResourceHandlePair Device::create_buffer(const std::string& name, const size_t size, void* data) {
        // Create engine resource
        const auto resource = std::make_shared<Resource>();
        *resource = {
            .type = ResourceType::buffer,
            .buffer_resource = {
                .data = data,
                .size = size,
            }
        };

        // Create Dx12 resource
        D3D12_RESOURCE_DESC resource_desc = {};
        resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resource_desc.Width = resource->expect_buffer().size;
        resource_desc.Height = 1;
        resource_desc.DepthOrArraySize = 1;
        resource_desc.MipLevels = 1;
        resource_desc.Format = DXGI_FORMAT_UNKNOWN;
        resource_desc.SampleDesc.Count = 1;
        resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
        
        D3D12_HEAP_PROPERTIES heap_properties = {
            D3D12_HEAP_TYPE_UPLOAD,
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            D3D12_MEMORY_POOL_UNKNOWN, 1, 1
        };

        validate(device->CreateCommittedResource(
            &heap_properties,
            D3D12_HEAP_FLAG_NONE,
            &resource_desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&resource->handle)
        ));

        // Allocate and create a shader resource view in the bindless descriptor heap
        const D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc {
            .Format = DXGI_FORMAT_R32_TYPELESS,
            .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Buffer = {
                .FirstElement = 0,
                .NumElements = static_cast<UINT>(size / 4),
                .StructureByteStride = 0,
                .Flags = D3D12_BUFFER_SRV_FLAG_RAW
            }
        };

        auto id = m_heap_bindless->alloc_descriptor(ResourceType::texture);
        const auto handle = m_heap_bindless->fetch_cpu_handle(id);
        device->CreateShaderResourceView(resource->handle.Get(), &srv_desc, handle);

        // Copy the buffer data to the GPU
        void* mapped_buffer = nullptr;
        D3D12_RANGE range = { 0, size };
        validate(resource->handle->Map(0, &range, &mapped_buffer));
        memcpy(mapped_buffer, data, size);
        resource->handle->Unmap(0, &range);

        auto name_str = std::wstring(name.begin(), name.end());
        resource->handle->SetName(name_str.c_str());

        // Store the resource data in the device struct
        id.is_loaded = true;
        m_resource_name_map[name] = id;
        m_resources[id.id] = resource;

        return ResourceHandlePair{ id, resource };
    }

    void Device::unload_bindless_resource(ResourceHandle id) {
        m_heap_bindless->free_descriptor(id);
    }

    bool Device::should_stay_open() {
        return glfwWindowShouldClose(m_window_glfw) == false;
    }

    void Device::set_full_screen(bool full_screen) {
        if (full_screen == true && m_is_fullscreen == false) {
            // Store window coords
            glfwGetWindowPos(m_window_glfw, &m_pos_x_pre_fullscreen, &m_pos_y_pre_fullscreen);
            glfwGetWindowSize(m_window_glfw, &m_width_pre_fullscreen, &m_height_pre_fullscreen);

            // Set to full screen
            int monitor_count;
            auto monitors = glfwGetMonitors(&monitor_count);
            auto monitor = monitors[find_dominant_monitor()];
            auto mode = glfwGetVideoMode(monitor);
            int x, y, w, h;
            glfwGetMonitorPos(monitor, &x, &y);
            w = mode->width;
            h = mode->height;
            printf("Display %i: %ix%i @ %ix%i\n", 0, w, h, x, y);
            glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
            glfwSetWindowSizeLimits(m_window_glfw, 256, 256, w, h);
            glfwSetWindowPos(m_window_glfw, x, y);
            glfwSetWindowSize(m_window_glfw, w, h);
        }
        else if (full_screen == false && m_is_fullscreen == true) {
            glfwSetWindowPos(m_window_glfw, m_pos_x_pre_fullscreen, m_pos_y_pre_fullscreen);
            glfwSetWindowSize(m_window_glfw, m_width_pre_fullscreen, m_height_pre_fullscreen);
            glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
        }
        m_is_fullscreen = full_screen;
    }

    int Device::find_dominant_monitor() {
        int monitor = 0;
        int best_score = 0;

        // Get window rectangle
        int wx0, wy0, wx1, wy1, w, h;
        glfwGetWindowPos(m_window_glfw, &wx0, &wy0);
        glfwGetWindowSize(m_window_glfw, &w, &h);
        wx1 = wx0 + w;
        wy1 = wy0 + h;

        // Loop over all monitors
        int n_monitors;
        auto monitors = glfwGetMonitors(&n_monitors);

        for (int i = 0; i < n_monitors; ++i) {
            // Get monitor rectangle
            int mx0, my0, mx1, my1;
            glfwGetMonitorPos(monitors[i], &mx0, &my0);
            auto mode = glfwGetVideoMode(monitors[i]);
            mx1 = mx0 + mode->width;
            my1 = my0 + mode->height;

            // Clip window to screen
            int cx0 = std::clamp(wx0, mx0, mx1);
            int cx1 = std::clamp(wx1, mx0, mx1);
            int cy0 = std::clamp(wy0, my0, my1);
            int cy1 = std::clamp(wy1, my0, my1);
            int cw = cx1 - cx0;
            int ch = cy1 - cy0;
            int area_on_monitor = cw * ch;

            // Remember the one that has the highest area
            if (area_on_monitor > best_score) {
                monitor = i;
                best_score = area_on_monitor;
            }
        }

        return monitor;
    }
}
