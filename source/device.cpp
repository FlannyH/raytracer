#define GLFW_EXPOSE_NATIVE_WIN32

#include "device.h"
#include <glfw/glfw3native.h>
#include <d3d12sdklayers.h>

#include <utility>

#include "buffer.h"
#include "command_buffer.h"
#include "swapchain.h"
#include "pipeline.h"
#include "descriptor_heap.h"
#include "command_queue.h"
#include "scene.h"
#include "input.h"
#include "fence.h"

namespace gfx {
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
            m_debug_layer = nullptr;
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
        m_heap_rtv = std::make_shared<DescriptorHeap>(*this, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 256);
        m_heap_dsv = std::make_shared<DescriptorHeap>(*this, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 256);
        m_heap_bindless = std::make_shared<DescriptorHeap>(*this, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1 / 2);

        // Init context
        m_queue_gfx = std::make_shared<CommandQueue>(*this, CommandBufferType::graphics, L"Graphics command queue");
        m_upload_queue = std::make_shared<CommandQueue>(*this, CommandBufferType::graphics, L"Upload command queue");
        m_swapchain = std::make_shared<Swapchain>(*this, *m_queue_gfx, *m_heap_rtv, m_framebuffer_format);
        m_upload_queue_completion_fence = std::make_shared<Fence>(*this);
        input::init(m_window_glfw);
        get_window_size(m_width, m_height);
    }

    Device::~Device() {
        // Finish any queued uploads
        m_upload_queue_completion_fence->gpu_signal(m_upload_queue, m_upload_fence_value_when_done);
        m_upload_queue->execute();
        m_upload_queue_completion_fence->cpu_wait(m_upload_fence_value_when_done);

        // Wait for GPU to finish
        m_swapchain->flush(m_queue_gfx);

        // Clean up in a certain order
        clean_up_old_resources();
        m_temp_upload_buffers.clear();
        m_swapchain.reset();
        m_curr_bound_pipeline.reset();
        m_curr_pass_cmd.reset();
        m_queue_gfx.reset();
        m_upload_queue.reset();
        m_upload_queue_completion_fence.reset();
        m_curr_render_targets.clear();
        m_curr_depth_target.reset();
        m_heap_rtv.reset();
        m_heap_dsv.reset();
        m_heap_bindless.reset();
        device->Release();
        factory->Release();
        glfwDestroyWindow(m_window_glfw);
    }

    void Device::resize_window(const int width, const int height) const {
        glfwSetWindowSize(m_window_glfw, width, height);
    }

    void Device::get_window_size(int& width, int& height) const {
        glfwGetWindowSize(m_window_glfw, &width, &height);
    }

    std::shared_ptr<Pipeline> Device::create_raster_pipeline(const std::string& vertex_shader_path, const std::string& pixel_shader_path, const std::initializer_list<ResourceHandlePair> render_targets, const ResourceHandlePair depth_target) {
        std::vector<DXGI_FORMAT> render_target_formats;
        DXGI_FORMAT depth_target_format = DXGI_FORMAT_UNKNOWN;

        // If we specify render targets, specify the formats
        for (auto& render_target : render_targets) {
            const auto& resource = render_target.resource;
            const auto& texture = resource->expect_texture();
            render_target_formats.emplace_back(pixel_format_to_dx12(texture.pixel_format));
        }

        // Otherwise, assume swapchain target and get its format
        if (render_target_formats.empty()) {
            render_target_formats.push_back(m_swapchain->curr_framebuffer()->GetDesc().Format);
        }

        // Get depth format
        if (depth_target.handle.type != (uint32_t)ResourceType::none) {
            const auto& resource = depth_target.resource;
            const auto& texture = resource->expect_texture();
            depth_target_format = pixel_format_to_dx12(texture.pixel_format);
        }

        return std::make_shared<Pipeline>(*this, vertex_shader_path, pixel_shader_path, render_target_formats, depth_target_format);
    }

    std::shared_ptr<Pipeline> Device::create_compute_pipeline(const std::string& compute_shader_path) {
        return std::make_shared<Pipeline>(*this, compute_shader_path);
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
        m_upload_queue_completion_fence->gpu_signal(m_upload_queue, m_upload_fence_value_when_done);
        m_upload_queue->execute();
        m_upload_queue_completion_fence->cpu_wait(m_upload_fence_value_when_done);
        m_swapchain->next_framebuffer();
        m_queue_gfx->clean_up_old_command_buffers(m_swapchain->current_fence_completed_value());
        m_upload_queue->clean_up_old_command_buffers(m_upload_fence_value_when_done);
        clean_up_old_resources();
    }

    void Device::end_frame() {
        m_swapchain->prepare_present(m_curr_pass_cmd);
        m_queue_gfx->execute();
        m_swapchain->synchronize(m_queue_gfx);
        m_swapchain->present();
        glfwPollEvents();
        glfwSwapBuffers(m_window_glfw);
    }

    void Device::set_graphics_root_constants(const std::vector<uint32_t>& constants) {
        auto gfx_cmd = m_curr_pass_cmd->get();
        UINT index = 0;
        for (const auto& constant : constants) {
            gfx_cmd->SetGraphicsRoot32BitConstant(0, constant, index++);
        }
    }

    void Device::set_compute_root_constants(const std::vector<uint32_t>& constants) {
        auto gfx_cmd = m_curr_pass_cmd->get();
        UINT index = 0;
        for (const auto& constant : constants) {
            gfx_cmd->SetComputeRoot32BitConstant(0, constant, index++);
        }
    }

    int Device::frame_index() {
        return m_swapchain->current_frame_index();
    }

    void Device::begin_raster_pass(std::shared_ptr<Pipeline> pipeline, RasterPassInfo&& render_pass_info) {
        // Create command buffer for this pass
        m_curr_pass_cmd = m_queue_gfx->create_command_buffer(*this, pipeline.get(), m_swapchain->current_frame_index());

        // Set up pipeline
        m_curr_bound_pipeline = pipeline;
        ID3D12DescriptorHeap* heaps[] = {
            m_heap_bindless->heap.Get(),
        };
        m_curr_pass_cmd->get()->SetDescriptorHeaps(1, heaps);
        m_curr_pass_cmd->get()->SetPipelineState(m_curr_bound_pipeline->pipeline_state.Get());
        m_curr_pass_cmd->get()->SetGraphicsRootSignature(m_curr_bound_pipeline->root_signature.Get());

        D3D12_VIEWPORT viewport{};
        D3D12_RECT scissor{};
        bool have_rtv = false;
        bool have_dsv = false;
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtv_handles;
        D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle{};

        // If the color target is the swapchain, prepare the swapchain for that
        if (render_pass_info.color_targets.empty()) {
            m_swapchain->prepare_render(m_curr_pass_cmd);
            viewport = {
                .TopLeftX = 0.0f,
                .TopLeftY = 0.0f,
                .Width = (FLOAT)m_width,
                .Height = (FLOAT)m_height,
                .MinDepth = 0.0f,
                .MaxDepth = 1.0f,
            };
            scissor = {
                .left = 0,
                .top = 0,
                .right = (LONG)m_width,
                .bottom = (LONG)m_height,
            };
            rtv_handles.push_back(m_swapchain->curr_framebuffer_rtv());
            have_rtv = true;
        }

        // Otherwise, if the color target is a texture, transition the texture to render target, and then bind it
        else {
            for (auto& color_target : render_pass_info.color_targets) {
                auto& texture = color_target.resource;
                auto gfx_cmd = m_curr_pass_cmd->get();

                transition_resource(m_curr_pass_cmd.get(), texture.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
                m_curr_render_targets.push_back(texture);
                auto rtv_handle = m_heap_rtv->fetch_cpu_handle(texture->expect_texture().rtv_handle);
                rtv_handles.push_back(rtv_handle);

                viewport = {
                    .TopLeftX = 0.0f,
                    .TopLeftY = 0.0f,
                    .Width = (FLOAT)texture->expect_texture().width,
                    .Height = (FLOAT)texture->expect_texture().height,
                    .MinDepth = 0.0f,
                    .MaxDepth = 1.0f,
                };
                scissor = {
                    .left = 0,
                    .top = 0,
                    .right = (LONG)texture->expect_texture().width,
                    .bottom = (LONG)texture->expect_texture().height,
                };
                if (render_pass_info.clear_on_begin) {
                    m_curr_pass_cmd->get()->ClearRenderTargetView(rtv_handle, &texture->expect_texture().clear_color.x, 0, nullptr);
                }
            }
            have_rtv = true;
        }

        // If we have a depth buffer, bind it too
        if ((ResourceType)render_pass_info.depth_target.handle.type == ResourceType::texture) {
            auto& texture = render_pass_info.depth_target.resource;
            auto gfx_cmd = m_curr_pass_cmd->get();

            transition_resource(m_curr_pass_cmd.get(), texture.get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
            dsv_handle = m_heap_dsv->fetch_cpu_handle(texture->expect_texture().dsv_handle);
            m_curr_depth_target = texture;
            m_curr_pass_cmd->get()->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, texture->expect_texture().clear_color.x, 0, 0, nullptr);

            have_dsv = true;
        }
        
        m_curr_pass_cmd->get()->RSSetViewports(1, &viewport);
        m_curr_pass_cmd->get()->RSSetScissorRects(1, &scissor);
        m_curr_pass_cmd->get()->OMSetRenderTargets(
            have_rtv ? (UINT)rtv_handles.size() : 0,
            have_rtv ? rtv_handles.data() : nullptr,
            false, 
            have_dsv ? &dsv_handle : nullptr
        );
    }

    void Device::end_raster_pass() {
        // If the current render target wasn't the swapchain, the m_curr_render_target pointer is not null
        // In this case, transition it back to a shader resource, so we can run compute shaders on it if we want, or blit it to the swapchain
        for (auto& render_target : m_curr_render_targets) {
            transition_resource(m_curr_pass_cmd.get(), render_target.get(), D3D12_RESOURCE_STATE_COMMON);
        }
        m_curr_render_targets.clear();

        if (m_curr_depth_target.get() != nullptr) {
            transition_resource(m_curr_pass_cmd.get(), m_curr_depth_target.get(), D3D12_RESOURCE_STATE_COMMON);
            m_curr_depth_target = nullptr;
        }
    }

    void Device::begin_compute_pass(std::shared_ptr<Pipeline> pipeline) {
        // Create command buffer for this pass
        m_curr_pass_cmd = m_queue_gfx->create_command_buffer(*this, pipeline.get(), m_swapchain->current_frame_index());

        // Set up pipeline
        m_curr_bound_pipeline = pipeline;
        ID3D12DescriptorHeap* heaps[] = {
            m_heap_bindless->heap.Get(),
        };
        m_curr_pass_cmd->get()->SetDescriptorHeaps(1, heaps);
        m_curr_pass_cmd->get()->SetPipelineState(m_curr_bound_pipeline->pipeline_state.Get());
        m_curr_pass_cmd->get()->SetComputeRootSignature(m_curr_bound_pipeline->root_signature.Get());
    }

    void Device::end_compute_pass() {}

    void Device::dispatch_threadgroups(uint32_t x, uint32_t y, uint32_t z) {
        m_curr_pass_cmd->get()->Dispatch(x, y, z);
    }

    void Device::draw_vertices(uint32_t n_vertices) {
        if (!m_curr_bound_pipeline) {
            printf("[ERROR] Attempt to record draw call without a pipeline set! Did you forget to call `begin_raster_pass()`?\n");
            return;
        }

        // Get command buffer
        auto gfx_cmd = m_curr_pass_cmd->get();

        // Record draw call
        gfx_cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        gfx_cmd->DrawInstanced(n_vertices, 1, 0, 0);
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
                .pixel_format = pixel_format,
            }
        };

        // Create a d3d12 resource for the texture
        D3D12_RESOURCE_DESC resource_desc = {};
        resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resource_desc.Width = resource->expect_texture().width;
        resource_desc.Height = resource->expect_texture().height;
        resource_desc.DepthOrArraySize = 1;
        resource_desc.MipLevels = 1;
        resource_desc.Format = pixel_format_to_dx12(pixel_format);
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
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&resource->handle)
        ));
        resource->current_state = D3D12_RESOURCE_STATE_COPY_DEST;
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
        if (data) {
            const auto upload_buffer_id = create_buffer("Upload buffer", upload_size, data);
            queue_unload_bindless_resource(upload_buffer_id);

            const auto& upload_buffer = upload_buffer_id.resource;

            void* mapped_buffer;
            const D3D12_RANGE upload_range = { 0, upload_size };
            validate(upload_buffer->handle->Map(0, &upload_range, &mapped_buffer));
            memcpy(mapped_buffer, data, upload_size);
            upload_buffer->handle->Unmap(0, &upload_range);

            const auto texture_size_box = D3D12_BOX{
                .left = 0,
                .top = 0,
                .front = 0,
                .right = width,
                .bottom = height,
                .back = 1,
            };

            const auto texture_copy_source = D3D12_TEXTURE_COPY_LOCATION{
                .pResource = upload_buffer->handle.Get(),
                .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
                .PlacedFootprint = {
                    .Offset = 0,
                    .Footprint = {
                        .Format = pixel_format_to_dx12(resource->expect_texture().pixel_format),
                        .Width = resource->expect_texture().width,
                        .Height = resource->expect_texture().height,
                        .Depth = 1,
                        .RowPitch = resource->expect_texture().width * static_cast<uint32_t>(size_per_pixel(pixel_format)),
                    }
                }
            };

            const auto texture_copy_dest = D3D12_TEXTURE_COPY_LOCATION{
                .pResource = resource->handle.Get(),
                .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
                .SubresourceIndex = 0,
            };

            // todo: determine whether i want this to be in a separate update_texture() function
            ++m_upload_fence_value_when_done;
            const auto upload_command_buffer = m_upload_queue->create_command_buffer(*this, nullptr, m_upload_fence_value_when_done);
            const auto cmd = upload_command_buffer->get();
            cmd->CopyTextureRegion(&texture_copy_dest, 0, 0, 0, &texture_copy_source, &texture_size_box);
            transition_resource(upload_command_buffer.get(), resource.get(), D3D12_RESOURCE_STATE_COMMON);
            m_temp_upload_buffers.push_back(UploadQueueKeepAlive{ m_upload_fence_value_when_done, upload_buffer });
        }

        id.is_loaded = true; // todo, only set this to true when the upload command buffer finished (when the fence value was reached)

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

        if (data) {
            // Copy the buffer data to the GPU
            void* mapped_buffer = nullptr;
            D3D12_RANGE range = { 0, size };
            validate(resource->handle->Map(0, &range, &mapped_buffer));
            memcpy(mapped_buffer, data, size);
            resource->handle->Unmap(0, &range);
        }

        auto name_str = std::wstring(name.begin(), name.end());
        resource->handle->SetName(name_str.c_str());
        resource->name = name;

        // Store the resource data in the device struct
        id.is_loaded = true;

        return ResourceHandlePair{ id, resource };
    }

    ResourceHandlePair Device::create_render_target(const std::string& name, uint32_t width, uint32_t height, PixelFormat pixel_format, glm::vec4 clear_color) {
        // Make texture resource
        const auto resource = std::make_shared<Resource>();
        *resource = {
            .type = ResourceType::texture,
            .texture_resource = {
                .data = nullptr,
                .width = width,
                .height = height,
                .pixel_format = pixel_format,
                .clear_color = clear_color,
                .rtv_handle = ResourceHandle::none(),
                .dsv_handle = ResourceHandle::none(),
            }
        };

        // Create a d3d12 resource for the texture
        D3D12_RESOURCE_DESC resource_desc = {};
        resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resource_desc.Width = resource->expect_texture().width;
        resource_desc.Height = resource->expect_texture().height;
        resource_desc.DepthOrArraySize = 1;
        resource_desc.MipLevels = 1;
        resource_desc.Format = pixel_format_to_dx12(pixel_format);
        resource_desc.SampleDesc.Count = 1;
        resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        D3D12_CLEAR_VALUE clear_value = {
            .Format = resource_desc.Format,
            .Color = { clear_color.r, clear_color.g, clear_color.b, clear_color.a },
        };

        // Create resource
        D3D12_HEAP_PROPERTIES heap_properties = {};
        heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
        validate(device->CreateCommittedResource(
            &heap_properties,
            D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
            &resource_desc,
            D3D12_RESOURCE_STATE_COMMON,
            &clear_value,
            IID_PPV_ARGS(&resource->handle)
        ));
        resource->current_state = D3D12_RESOURCE_STATE_COMMON;
        auto name_str = std::wstring(name.begin(), name.end());
        resource->handle->SetName(name_str.c_str());
        resource->name = name;

        // todo: make this its own function or combine this function using some type of flag, I've repeated this 3 times now
        // Create SRV
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
        auto srv_id = m_heap_bindless->alloc_descriptor(ResourceType::texture);
        auto srv_descriptor = m_heap_bindless->fetch_cpu_handle(srv_id);
        device->CreateShaderResourceView(resource->handle.Get(), &srv_desc, srv_descriptor);
        srv_id.is_loaded = true;

        // Create RTV
        D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {
            .Format = resource_desc.Format,
            .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
            .Texture2D = {
                .MipSlice = 0,
                .PlaneSlice = 0,
            }
        };
        auto rtv_id = m_heap_rtv->alloc_descriptor(ResourceType::texture);
        auto rtv_descriptor = m_heap_rtv->fetch_cpu_handle(rtv_id);
        device->CreateRenderTargetView(resource->handle.Get(), &rtv_desc, rtv_descriptor);
        rtv_id.is_loaded = true;
        resource->expect_texture().rtv_handle = rtv_id;

        return ResourceHandlePair{ srv_id, resource };
    }

    ResourceHandlePair Device::create_depth_target(const std::string& name, uint32_t width, uint32_t height, PixelFormat pixel_format, float clear_depth) {
        // Make texture resource
        const auto resource = std::make_shared<Resource>();
        *resource = {
            .type = ResourceType::texture,
            .texture_resource = {
                .data = nullptr,
                .width = width,
                .height = height,
                .pixel_format = pixel_format,
                .clear_color = glm::vec4(clear_depth, 0.0f, 0.0f, 1.0f),
                .rtv_handle = ResourceHandle::none(),
                .dsv_handle = ResourceHandle::none(),
            }
        };

        // Create a d3d12 resource for the texture
        D3D12_RESOURCE_DESC resource_desc = {};
        resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resource_desc.Width = resource->expect_texture().width;
        resource_desc.Height = resource->expect_texture().height;
        resource_desc.DepthOrArraySize = 1;
        resource_desc.MipLevels = 1;
        resource_desc.Format = pixel_format_to_dx12(pixel_format);
        resource_desc.SampleDesc.Count = 1;
        resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        D3D12_CLEAR_VALUE clear_value = {
            .Format = resource_desc.Format,
            .DepthStencil = {
                .Depth = 1.0f,
                .Stencil = 0,
            }
        };

        // Create resource
        D3D12_HEAP_PROPERTIES heap_properties = {};
        heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
        validate(device->CreateCommittedResource(
            &heap_properties,
            D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
            &resource_desc,
            D3D12_RESOURCE_STATE_COMMON,
            &clear_value,
            IID_PPV_ARGS(&resource->handle)
        ));
        resource->current_state = D3D12_RESOURCE_STATE_COMMON;
        auto name_str = std::wstring(name.begin(), name.end());
        resource->handle->SetName(name_str.c_str());
        resource->name = name;

        // Allocate SRV id, so we can store it in the resources map
        // However, since this is a depth texture, we can't actually create a SRV for this
        auto srv_id = m_heap_bindless->alloc_descriptor(ResourceType::texture);

        // Create DSV
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {
            .Format = resource_desc.Format,
            .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
            .Flags = D3D12_DSV_FLAG_NONE,
            .Texture2D = {
                .MipSlice = 0,
            }
        };
        auto dsv_id = m_heap_dsv->alloc_descriptor(ResourceType::texture);
        auto dsv_descriptor = m_heap_dsv->fetch_cpu_handle(dsv_id);
        device->CreateDepthStencilView(resource->handle.Get(), &dsv_desc, dsv_descriptor);
        dsv_id.is_loaded = true;
        resource->expect_texture().dsv_handle = dsv_id;

        return ResourceHandlePair{ srv_id, resource };
    }

    void Device::resize_texture(ResourceHandlePair& handle, const uint32_t width, const uint32_t height) {
        // Get texture info (is it a render target, depth target, or a regular texture?)
        auto& resource = handle.resource;
        auto& texture = resource->expect_texture();
        assert(!((texture.rtv_handle.type != (uint32_t)ResourceType::none) && (texture.dsv_handle.type != (uint32_t)ResourceType::none)) && "Invalid texture: both rtv_handle and dsv_handle are set!");
        
        // We'll store the newly created texture here
        ResourceHandle new_handle = ResourceHandle::none();

        // If it's a render target, create a new render target
        if (texture.rtv_handle.type != (uint32_t)ResourceType::none) {
            handle = create_render_target(resource->name, width, height, texture.pixel_format, texture.clear_color);
            return;
        }

        // If it's a depth target, create a new depth target
        if (texture.dsv_handle.type != (uint32_t)ResourceType::none) {
            handle = create_depth_target(resource->name, width, height, texture.pixel_format, texture.clear_color.r);
            return;
        }

        // todo: If it's not a render target, copy the data over
        abort();
    }

    void Device::update_buffer(const ResourceHandlePair& buffer, const uint32_t offset, const uint32_t size_bytes, const void* data) {
        // todo: handle case for buffers that are not visible from cpu
        char* mapped_buffer;
        const D3D12_RANGE read_range = { 0, 0 };
        validate(buffer.resource->handle->Map(0, &read_range, (void**)&mapped_buffer));
        memcpy(mapped_buffer + offset, data, size_bytes);
        buffer.resource->handle->Unmap(0, &read_range);
    }

    void Device::queue_unload_bindless_resource(ResourceHandlePair resource) {
        int fence_value = m_swapchain->current_frame_index() + 3;
        m_resources_to_unload.push_back({ resource, fence_value });
    }

    void Device::transition_resource(CommandBuffer* cmd, Resource* resource, D3D12_RESOURCE_STATES new_state) {
        if (resource->current_state == new_state) return;

        const D3D12_RESOURCE_BARRIER barrier = {
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .Transition = {
                .pResource = resource->handle.Get(),
                .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                .StateBefore = resource->current_state,
                .StateAfter = new_state,
            },
        };
        cmd->get()->ResourceBarrier(1, &barrier);
        resource->current_state = new_state;
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

    void Device::clean_up_old_resources() {
        while (!m_resources_to_unload.empty()) {
            // Get the next entry in the unload queue
            auto& [resource, desired_completed_fence_value] = m_resources_to_unload.front();

            // If it's still potentially being used, end the loop here, because all subsequent queue entries have
            // the equal or higher desired fence values
            if ((int)m_swapchain->current_fence_completed_value() < desired_completed_fence_value) break;

            // Destroy, Erase, Improve (memory usage) - good Meshuggah album btw, go listen to it
            assert(resource.resource->handle != nullptr && "Critical error! Somehow the handle became null. Check the other code, this is sus.");
            m_resources_to_unload.pop_front();
        }

        while (!m_temp_upload_buffers.empty()) {
            // Get the next entry in the unload queue
            auto& upload_data = m_temp_upload_buffers.front();

            // If it's still potentially being used, end the loop here, because all subsequent queue entries have
            // the equal or higher desired fence values
            if (m_upload_queue_completion_fence->reached_value(upload_data.upload_queue_fence_value) == false) break;

            // Destroy, Erase, Improve (memory usage) - good Meshuggah album btw, go listen to it
            assert(upload_data.upload_buffer->handle != nullptr && "Critical error! Somehow the handle became null. Check the other code, this is sus.");
            m_temp_upload_buffers.pop_front();
        }
    }
}
