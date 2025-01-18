#define GLFW_EXPOSE_NATIVE_WIN32

#include "device.h"
#include <glfw/glfw3native.h>
#include <d3d12sdklayers.h>

#include <algorithm>
#include <utility>
#include <mutex>

#include "buffer.h"
#include "command_buffer.h"
#include "swapchain.h"
#include "pipeline.h"
#include "descriptor_heap.h"
#include "command_queue.h"
#include "scene.h"
#include "input.h"
#include "fence.h"
#include <winerror.h>

#define MAX_QUERY_COUNT 128
#define DEBUG_PRINT_GPU_PROFILING 0

namespace gfx {
    const char* breadcrumb_op_names[49] = {
        "SETMARKER",                                        "BEGINEVENT",
        "ENDEVENT",                                         "DRAWINSTANCED",
        "DRAWINDEXEDINSTANCED",                             "EXECUTEINDIRECT",
        "DISPATCH",                                         "COPYBUFFERREGION",
        "COPYTEXTUREREGION",                                "COPYRESOURCE",
        "COPYTILES",                                        "RESOLVESUBRESOURCE",
        "CLEARRENDERTARGETVIEW",                            "CLEARUNORDEREDACCESSVIEW",
        "CLEARDEPTHSTENCILVIEW",                            "RESOURCEBARRIER",
        "EXECUTEBUNDLE",                                    "PRESENT",
        "RESOLVEQUERYDATA",                                 "BEGINSUBMISSION",
        "ENDSUBMISSION",                                    "DECODEFRAME",
        "PROCESSFRAMES",                                    "ATOMICCOPYBUFFERUINT",
        "ATOMICCOPYBUFFERUINT64",                           "RESOLVESUBRESOURCEREGION",
        "WRITEBUFFERIMMEDIATE",                             "DECODEFRAME1",
        "SETPROTECTEDRESOURCESESSION",                      "DECODEFRAME2",
        "PROCESSFRAMES1",                                   "BUILDRAYTRACINGACCELERATIONSTRUCTURE",
        "EMITRAYTRACINGACCELERATIONSTRUCTUREPOSTBUILDINFO", "COPYRAYTRACINGACCELERATIONSTRUCTURE",
        "DISPATCHRAYS",                                     "INITIALIZEMETACOMMAND",
        "EXECUTEMETACOMMAND",                               "ESTIMATEMOTION",
        "RESOLVEMOTIONVECTORHEAP",                          "SETPIPELINESTATE1",
        "INITIALIZEEXTENSIONCOMMAND",                       "EXECUTEEXTENSIONCOMMAND",
        "DISPATCHMESH",                                     "ENCODEFRAME",
        "RESOLVEENCODEROUTPUTMETADATA",                     "BARRIER",
        "BEGIN_COMMAND_LIST",                               "DISPATCHGRAPH",
        "SETPROGRAM"
    };

    struct ThreadSharedGlobals {
        bool should_shut_down = false;
        std::shared_ptr<Fence> device_lost_fence = nullptr;
    } thread_shared_globals;

    std::mutex mutex_thread_shared_globals;

    static void validation_message_callback(D3D12_MESSAGE_CATEGORY category, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID id, LPCSTR description, void* context) {
        const char* category_names[] = {
            "Application Defined", 
            "Miscellaneous",
            "Initialization",
            "Cleanup",
            "Compilation",
            "State Creation",
            "State Setting",
            "State Getting",
            "Resource Manipulation",
            "Execution",
            "Shader",
        };

        Log::Level level = Log::Level::Disabled;
        switch (severity) {
        case D3D12_MESSAGE_SEVERITY_CORRUPTION:
        case D3D12_MESSAGE_SEVERITY_ERROR:
            level = Log::Level::Error;
            break;
        case D3D12_MESSAGE_SEVERITY_WARNING:
            level = Log::Level::Warning;
            break;
        case D3D12_MESSAGE_SEVERITY_INFO:
            level = Log::Level::Info;
            break;
        case D3D12_MESSAGE_SEVERITY_MESSAGE:
            level = Log::Level::Debug;
            break;
        }

        Log::write(level, "D3D12: %s: %s", category_names[category], description);
    }

    Device::Device(const int width, const int height, const bool debug_layer_enabled, const bool gpu_profiling_enabled) {
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
            ComPtr<ID3D12Debug1> debug_interface = nullptr;
            validate(m_debug_layer->QueryInterface(IID_PPV_ARGS(&debug_interface)));
            debug_interface->EnableDebugLayer();
            debug_interface->SetEnableGPUBasedValidation(true);
            debug_interface->Release();
            debug_interface = nullptr;
        }

#ifdef _DEBUG
        ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> dred_settings;
        if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dred_settings)))) {
            dred_settings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dred_settings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        }
#endif

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
                LOG(Info, "Ignoring device \"%ws\"", desc.Description);
                continue;
            }

            // Does this adapter support Direct3D 12.0?
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)))) {
                // Yes it does! We use this one.
                LOG(Info, "Using device \"%ws\"", desc.Description);
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

        // Register the validation message callback
        if (debug_layer_enabled) {
            ComPtr<ID3D12InfoQueue1> info_queue;
            validate(device->QueryInterface(IID_PPV_ARGS(&info_queue)));
            info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
            info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
            info_queue->RegisterMessageCallback(validation_message_callback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &m_msg_callback_cookie);
            info_queue->Release();
        }

        // Create descriptor heaps
        m_heap_rtv = std::make_shared<DescriptorHeap>(*this, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 256);
        m_heap_dsv = std::make_shared<DescriptorHeap>(*this, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 256);
        m_heap_bindless = std::make_shared<DescriptorHeap>(*this, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1 / 2);

        // Init context
        m_queue_gfx = std::make_shared<CommandQueue>(device.Get(), CommandBufferType::graphics, L"Graphics command queue");
        m_upload_queue = std::make_shared<CommandQueue>(device.Get(), CommandBufferType::compute, L"Upload command queue");
        m_swapchain = std::make_shared<Swapchain>(*this, *m_queue_gfx, *m_heap_rtv, m_framebuffer_format);
        m_upload_queue_completion_fence = std::make_shared<Fence>(*this);

        {
            std::lock_guard<std::mutex> lock(mutex_thread_shared_globals);
            thread_shared_globals.device_lost_fence = std::make_shared<Fence>(*this);
        }
        auto device_lost_handler = [](Device* device) {
            LOG(Debug, "Device removal handler thread created");

            // Do nothing until device removal or shutdown exists
            HANDLE device_lost_fence = nullptr;
            {
                std::lock_guard<std::mutex> lock(mutex_thread_shared_globals);
                auto& fence = thread_shared_globals.device_lost_fence->fence;
                auto value = UINT64_MAX;
                if (fence->GetCompletedValue() < value) {
                    device_lost_fence = thread_shared_globals.device_lost_fence->event_handle;
                    validate(fence->SetEventOnCompletion(value, device_lost_fence));
                };                
            }
            if (device_lost_fence) {
                WaitForSingleObject(device_lost_fence, INFINITE);
            }

            // If the renderer shut down, it will have set the thread_shared_globals.should_shut_down flag
            {
                std::lock_guard<std::mutex> lock(mutex_thread_shared_globals);
                if (thread_shared_globals.should_shut_down) {
                    LOG(Debug, "Device removal thread shutting down");
                    return;
                }
            }
            
            // Debug printe
            ComPtr<ID3D12DeviceRemovedExtendedData> pDred;
            validate(device->device->QueryInterface(IID_PPV_ARGS(&pDred)));
            D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT DredAutoBreadcrumbsOutput;
            D3D12_DRED_PAGE_FAULT_OUTPUT DredPageFaultOutput;
            validate(pDred->GetAutoBreadcrumbsOutput(&DredAutoBreadcrumbsOutput));
            validate(pDred->GetPageFaultAllocationOutput(&DredPageFaultOutput));
            LOG(Fatal, "Device removal detected!");
            printf("Breadcrumbs: \n");
            auto* curr_node = DredAutoBreadcrumbsOutput.pHeadAutoBreadcrumbNode;
            int node_index = 0;
            while (curr_node) {
                printf("    Node %i: \n", node_index++);
                if (curr_node->pCommandListDebugNameA) printf("        pCommandListDebugNameA: %s\n", curr_node->pCommandListDebugNameA);
                if (curr_node->pCommandListDebugNameW) printf("        pCommandListDebugNameW: %ws\n", curr_node->pCommandListDebugNameW);
                if (curr_node->pCommandQueueDebugNameA) printf("        pCommandQueueDebugNameA: %s\n", curr_node->pCommandQueueDebugNameA);
                if (curr_node->pCommandQueueDebugNameW) printf("        pCommandQueueDebugNameW: %ws\n", curr_node->pCommandQueueDebugNameW);
                for (uint32_t i = 0; i < curr_node->BreadcrumbCount; ++i) {
                    const char* command_history = breadcrumb_op_names[curr_node->pCommandHistory[i]];
                    printf("        %i: command: %s", i, command_history);
                    if (i < *curr_node->pLastBreadcrumbValue) printf(" (completed)");
                    else if (i == *curr_node->pLastBreadcrumbValue) printf(" <-- last completed operation");
                    printf("\n");
                }
                printf("\n");
                curr_node = curr_node->pNext;
            }

            LOG(Debug, "Device removal thread shutting down");
        };
        device_lost_thread = std::thread(device_lost_handler, this);
        
        m_gpu_profiling = debug_layer_enabled;
        if (m_gpu_profiling) {
            device->SetStablePowerState(true);

            D3D12_QUERY_HEAP_DESC query_heap_desc = {};
            query_heap_desc.Count = MAX_QUERY_COUNT * 2;
            query_heap_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
            device->CreateQueryHeap(&query_heap_desc, IID_PPV_ARGS(&m_query_heap));
            m_query_buffer = create_buffer("GPU profiling query buffer", MAX_QUERY_COUNT * sizeof(uint64_t), nullptr, ResourceUsage::cpu_read_write);
            uint64_t freq;
            m_queue_gfx->command_queue->GetTimestampFrequency(&freq);
            m_timestamp_frequency = (float)freq;
        }

        input::init(m_window_glfw);
        get_window_size(m_width, m_height);
    }

    Device::~Device() {
        // Kill the breadcrumb thread
        {
            std::lock_guard<std::mutex> lock = std::lock_guard(mutex_thread_shared_globals);
            thread_shared_globals.should_shut_down = true;
            thread_shared_globals.device_lost_fence->cpu_signal(UINT64_MAX);
        }
        // Wait for device removal thread to return
        device_lost_thread.join();

        // Finish any queued uploads
        m_upload_queue_completion_fence->gpu_signal(m_upload_queue, m_upload_fence_value_when_done);
        m_upload_queue->execute();
        m_upload_queue_completion_fence->cpu_wait(m_upload_fence_value_when_done);

        // Wait for GPU to finish
        m_swapchain->flush(m_queue_gfx);

        // Clean up in a certain order
        while (!m_resources_to_unload.empty() || !m_temp_upload_buffers.empty()) {
            begin_frame();
            end_frame();
            clean_up_old_resources();
        }
        m_temp_upload_buffers.clear();
        m_swapchain.reset();
        m_curr_bound_pipeline.reset();
        m_curr_pass_cmd.reset();
        m_queue_gfx.reset();
        m_upload_queue.reset();
        m_upload_queue_completion_fence.reset();
        m_heap_rtv.reset();
        m_heap_dsv.reset();
        m_heap_bindless.reset();
        m_query_heap.Reset();
        device.Reset();
        factory.Reset();
        glfwDestroyWindow(m_window_glfw);
    }

    void Device::resize_window(const int width, const int height) const {
        glfwSetWindowSize(m_window_glfw, width, height);
    }

    void Device::get_window_size(int& width, int& height) const {
        glfwGetWindowSize(m_window_glfw, &width, &height);
    }

    std::shared_ptr<Pipeline> Device::create_raster_pipeline(const std::string& name, const std::string& vertex_shader_path, const std::string& pixel_shader_path, const std::initializer_list<ResourceHandlePair> render_targets, const ResourceHandlePair depth_target) {
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

        return std::make_shared<Pipeline>(*this, name, vertex_shader_path, pixel_shader_path, render_target_formats, depth_target_format);
    }

    std::shared_ptr<Pipeline> Device::create_compute_pipeline(const std::string& name, const std::string& compute_shader_path) {
        return std::make_shared<Pipeline>(*this, name, compute_shader_path);
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
        m_upload_queue_completion_fence->gpu_signal(m_upload_queue, m_upload_fence_value_when_done);
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

        if (m_gpu_profiling) {
            std::vector<uint64_t> timestamps;
            timestamps.resize(m_query_labels.size() * 2);
            readback_buffer(m_query_buffer, 0, timestamps.size() * sizeof(uint64_t), timestamps.data());
            std::vector<float> pipeline_times;
            pipeline_times.resize(m_query_labels.size());
            float total = 0.0f;
            for (int i = 0; i < m_query_labels.size(); ++i) {
                pipeline_times[i] = ((float)(timestamps[i*2 + 1] - timestamps[i*2 + 0])) / m_timestamp_frequency;
                total += pipeline_times[i];
            }

#if DEBUG_PRINT_GPU_PROFILING
            LOG(Debug, "----------------------------------------GPU PROFILING----------------------------------------");
            for (int i = 0; i < m_query_labels.size(); ++i) {
                LOG(Debug, "%56s: %.3f ms (%2.1f%%)", m_query_labels[i].c_str(), pipeline_times[i] * 1000.f, 100.f * pipeline_times[i] / total);
            }

            // Warn if we drop below 60 fps, error if we drop below 30 fps, cuz then there's 
            LOG(Debug,    "Frame: %.3f ms (%.1f fps)", total * 1000.f, 1.0f / total);
            LOG(Debug, "---------------------------------------------------------------------------------------------\n");
#endif
            m_query_labels.clear();
        }
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

    bool Device::supports(RendererFeature feature) {
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 feature_opt5{};

        switch (feature) {
            case RendererFeature::raytracing:
                if (FAILED(device->CheckFeatureSupport(
                    D3D12_FEATURE_D3D12_OPTIONS5,
                    &feature_opt5,
                    sizeof(feature_opt5)
                ))) return false;
                return (feature_opt5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED);
            default: 
                return false;
        }
    }

    void Device::begin_raster_pass(std::shared_ptr<Pipeline> pipeline, RasterPassInfo&& render_pass_info) {
        // Create command buffer for this pass
        m_curr_pass_cmd = m_queue_gfx->create_command_buffer(pipeline.get(), m_swapchain->current_frame_index());

        if (m_gpu_profiling) {
            if (m_query_labels.size() * 2 > MAX_QUERY_COUNT) {
                LOG(Error, "Query buffer overflow");
            }
            m_curr_pass_cmd->get()->EndQuery(m_query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, m_query_labels.size() * 2); // Entries are stored in pairs
        }

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

                transition_resource(m_curr_pass_cmd, texture, D3D12_RESOURCE_STATE_RENDER_TARGET);
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
                if (render_pass_info.clear_on_begin && texture->expect_texture().clear_on_begin) {
                    transition_resource(m_curr_pass_cmd, texture, D3D12_RESOURCE_STATE_RENDER_TARGET);
                    execute_resource_transitions(m_curr_pass_cmd);
                    auto clear_color = &texture->expect_texture().clear_color;
                    m_curr_pass_cmd->get()->ClearRenderTargetView(rtv_handle, &clear_color->r, 0, nullptr);
                }
            }
            have_rtv = true;
        }

        // If we have a depth buffer, bind it too
        if ((ResourceType)render_pass_info.depth_target.handle.type == ResourceType::texture) {
            auto& texture = render_pass_info.depth_target.resource;
            auto gfx_cmd = m_curr_pass_cmd->get();

            transition_resource(m_curr_pass_cmd, texture, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            dsv_handle = m_heap_dsv->fetch_cpu_handle(texture->expect_texture().dsv_handle);
            m_curr_pass_cmd->get()->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, texture->expect_texture().clear_color.x, 0, 0, nullptr);

            have_dsv = true;
        }
        
        execute_resource_transitions(m_curr_pass_cmd);
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
        if (m_gpu_profiling) {
            m_curr_pass_cmd->get()->EndQuery(m_query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, m_query_labels.size() * 2 + 1);
            m_query_labels.push_back(m_curr_bound_pipeline->get_name());
            transition_resource(m_curr_pass_cmd, m_query_buffer.resource, D3D12_RESOURCE_STATE_COPY_DEST);
            execute_resource_transitions(m_curr_pass_cmd);
            m_curr_pass_cmd->get()->ResolveQueryData(m_query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, m_query_labels.size() * 2, m_query_buffer.resource->handle.Get(), 0);
        }
    }

    void Device::begin_compute_pass(std::shared_ptr<Pipeline> pipeline, bool async) {
        // Create command buffer for this pass
        if (async) {
            m_curr_pass_cmd = m_upload_queue->create_command_buffer(pipeline.get(), ++m_upload_fence_value_when_done);
            m_curr_pipeline_is_async = true;
        }
        else {
            m_curr_pass_cmd = m_queue_gfx->create_command_buffer(pipeline.get(), m_swapchain->current_frame_index());
        }

        if (m_gpu_profiling) {
            if (m_query_labels.size() * 2 > MAX_QUERY_COUNT) {
                LOG(Error, "Query buffer overflow");
            }
            m_curr_pass_cmd->get()->EndQuery(m_query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, m_query_labels.size() * 2); // Entries are stored in pairs
        }

        // Set up pipeline
        m_curr_bound_pipeline = pipeline;
        ID3D12DescriptorHeap* heaps[] = {
            m_heap_bindless->heap.Get(),
        };
        m_curr_pass_cmd->get()->SetDescriptorHeaps(1, heaps);
        m_curr_pass_cmd->get()->SetPipelineState(m_curr_bound_pipeline->pipeline_state.Get());
        m_curr_pass_cmd->get()->SetComputeRootSignature(m_curr_bound_pipeline->root_signature.Get());
        m_curr_pass_cmd->get()->SetName(async ? L"Async compute pass" : L"Compute pass");	
    }

    void Device::end_compute_pass() {
        if (m_gpu_profiling) {
            m_curr_pass_cmd->get()->EndQuery(m_query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, m_query_labels.size() * 2 + 1);
            m_query_labels.push_back(m_curr_bound_pipeline->get_name());
            transition_resource(m_curr_pass_cmd, m_query_buffer.resource, D3D12_RESOURCE_STATE_COPY_DEST);
            execute_resource_transitions(m_curr_pass_cmd);
            m_curr_pass_cmd->get()->ResolveQueryData(m_query_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, m_query_labels.size() * 2, m_query_buffer.resource->handle.Get(), 0);
        }
    }

    void Device::dispatch_threadgroups(uint32_t x, uint32_t y, uint32_t z) {
        m_curr_pass_cmd->get()->Dispatch(x, y, z);
    }

    void Device::draw_vertices(uint32_t n_vertices) {
        if (!m_curr_bound_pipeline) {
            LOG(Error, "Attempt to record draw call without a pipeline set! Did you forget to call `begin_raster_pass()`?");
            return;
        }

        // Get command buffer
        auto gfx_cmd = m_curr_pass_cmd->get();

        // Record draw call
        gfx_cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        gfx_cmd->DrawInstanced(n_vertices, 1, 0, 0);
    }

    D3D12_UNORDERED_ACCESS_VIEW_DESC make_texture_uav_desc(DXGI_FORMAT format, TextureType type, int depth, int mip_slice) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {
            .Format = format,
        };

        switch (type) {
        case TextureType::tex_2d:
            uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uav_desc.Texture2D.MipSlice = mip_slice;
            uav_desc.Texture2D.PlaneSlice = 0;
            break;
        case TextureType::tex_3d:
            uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
            uav_desc.Texture3D.MipSlice = mip_slice;
            uav_desc.Texture3D.FirstWSlice = 0;
            uav_desc.Texture3D.WSize = depth;
            break;
        case TextureType::tex_cube:
            uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            uav_desc.Texture2DArray.MipSlice = mip_slice;
            uav_desc.Texture2DArray.FirstArraySlice = 0;
            uav_desc.Texture2DArray.ArraySize = 6;
            break;
        default:
            LOG(Error, "Unknown texture type %i", (int)type);
            break;
        }

        return uav_desc;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC make_texture_srv_desc(DXGI_FORMAT format, TextureType type, int mip_levels) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
            .Format = format,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        };
        switch (type) {
        case TextureType::tex_2d:
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MostDetailedMip = 0;
            srv_desc.Texture2D.MipLevels = mip_levels;
            srv_desc.Texture2D.PlaneSlice = 0;
            break;
        case TextureType::tex_3d:
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
            srv_desc.Texture3D.MostDetailedMip = 0;
            srv_desc.Texture3D.MipLevels = mip_levels;
            srv_desc.Texture3D.ResourceMinLODClamp = 0.0f;
            break;
        case TextureType::tex_cube:
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            srv_desc.TextureCube.MostDetailedMip = 0;
            srv_desc.TextureCube.MipLevels = mip_levels;
            srv_desc.TextureCube.ResourceMinLODClamp = 0.0f;
            break;
        default:
            LOG(Error, "Unknown texture type %i", (int)type);
            break;
        }

        return srv_desc;
    }

    ResourceHandlePair Device::load_texture(const std::string& name, uint32_t width, uint32_t height, uint32_t depth, void* data, PixelFormat pixel_format, TextureType type, ResourceUsage usage, int max_mip_levels, int min_resolution) {
        // Make texture resource
        const auto resource = std::make_shared<Resource>(ResourceType::texture);
        resource->usage = usage;
        resource->expect_texture() = {
            .data = static_cast<uint8_t*>(data),
            .width = width,
            .height = height,
            .depth = depth,
            .pixel_format = pixel_format,
            .is_compute_render_target = (usage == ResourceUsage::compute_write),
        };

        // Create a d3d12 resource for the texture
        D3D12_RESOURCE_DESC resource_desc = {};
        resource_desc.Dimension = texture_type_to_dx12_resource_dimension(type);
        resource_desc.Width = width;
        resource_desc.Height = height;
        resource_desc.DepthOrArraySize = depth;
        resource_desc.Format = pixel_format_to_dx12(pixel_format);
        resource_desc.SampleDesc.Count = 1;
        resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
        resource_desc.Flags |= (usage == ResourceUsage::compute_write) ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
        resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        ResourceHandle id = m_heap_bindless->alloc_descriptor(ResourceType::texture);

        int mip_levels = 1;
        if (max_mip_levels > 1) {
            uint32_t w = width;
            uint32_t h = height;
            while (w > min_resolution && h > min_resolution && mip_levels < max_mip_levels) {
                mip_levels++;
                w >>= 1;
                h >>= 1;
            }
        }
        resource_desc.MipLevels = (UINT16)mip_levels;

        D3D12_HEAP_PROPERTIES heap_properties = {};
        heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;

        const auto upload_size = width * height * depth * size_per_pixel(pixel_format);
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
        auto srv_desc = make_texture_srv_desc(resource_desc.Format, type, mip_levels);
        device->CreateShaderResourceView(resource->handle.Get(), &srv_desc, descriptor);
        
        std::vector<ResourceHandle> mip_handles;
        int mip_level = 0;
        if (max_mip_levels > 1) {
            uint32_t w = width;
            uint32_t h = height;
            while (++mip_level < mip_levels) {
                ResourceHandle mip_srv_id = m_heap_bindless->alloc_descriptor(ResourceType::texture);
                ResourceHandle mip_uav_id = mip_srv_id; mip_uav_id.id += 1;
                auto mip_srv_descriptor = m_heap_bindless->fetch_cpu_handle(mip_srv_id);
                auto mip_uav_descriptor = m_heap_bindless->fetch_cpu_handle(mip_uav_id);
                
                D3D12_SHADER_RESOURCE_VIEW_DESC mip_srv_desc = make_texture_srv_desc(resource_desc.Format, type, 1);
                D3D12_UNORDERED_ACCESS_VIEW_DESC mip_uav_desc = make_texture_uav_desc(resource_desc.Format, type, depth, mip_level);
                device->CreateShaderResourceView(resource->handle.Get(), &mip_srv_desc, mip_srv_descriptor);
                device->CreateUnorderedAccessView(resource->handle.Get(), nullptr, &mip_uav_desc, mip_uav_descriptor);

                mip_handles.push_back(mip_srv_id);

                w >>= 1;
                h >>= 1;
                if (w < 1) w = 1;
                if (h < 1) h = 1;
            }
        }

        if (usage == ResourceUsage::compute_write) {
            auto uav_id = id;
            uav_id.id += 1;
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = make_texture_uav_desc(resource_desc.Format, type, depth, 0);
            auto uav_descriptor = m_heap_bindless->fetch_cpu_handle(uav_id);
            device->CreateUnorderedAccessView(resource->handle.Get(), nullptr, &uav_desc, uav_descriptor);
        }

        // We need to copy the texture from an upload buffer
        if (data) {
            const auto upload_buffer_id = create_buffer("Upload buffer", upload_size, data, ResourceUsage::cpu_writable);
            queue_unload_bindless_resource(upload_buffer_id);

            const auto& upload_buffer = upload_buffer_id.resource;

            auto texture_size_box = D3D12_BOX{
                .left = 0,
                .top = 0,
                .front = 0,
                .right = width,
                .bottom = height,
                .back = depth,
            };

            auto texture_copy_source = D3D12_TEXTURE_COPY_LOCATION{
                .pResource = upload_buffer->handle.Get(),
                .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
                .PlacedFootprint = {
                    .Offset = 0,
                    .Footprint = {
                        .Format = pixel_format_to_dx12(resource->expect_texture().pixel_format),
                        .Width = resource->expect_texture().width,
                        .Height = resource->expect_texture().height,
                        .Depth = resource->expect_texture().depth,
                        .RowPitch = resource->expect_texture().width * static_cast<uint32_t>(size_per_pixel(pixel_format)),
                    }
                }
            };

            auto texture_copy_dest = D3D12_TEXTURE_COPY_LOCATION{
                .pResource = resource->handle.Get(),
                .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
                .SubresourceIndex = 0,
            };

            // todo: determine whether i want this to be in a separate update_texture() function
            ++m_upload_fence_value_when_done;
            const auto upload_command_buffer = m_upload_queue->create_command_buffer(nullptr, m_upload_fence_value_when_done);
            const auto cmd = upload_command_buffer->get();

            switch (type) {
            case TextureType::tex_2d:
            case TextureType::tex_3d:
                cmd->CopyTextureRegion(&texture_copy_dest, 0, 0, 0, &texture_copy_source, &texture_size_box);
                break;
            case TextureType::tex_cube:
                texture_copy_source.PlacedFootprint.Footprint.Depth = 1;
                texture_size_box.back = 1;
                for (uint32_t i = 0; i < 6; ++i) {
                    texture_copy_dest.SubresourceIndex = i;
                    texture_copy_source.PlacedFootprint.Offset = i * height * texture_copy_source.PlacedFootprint.Footprint.RowPitch;
                    cmd->CopyTextureRegion(&texture_copy_dest, 0, 0, 0, &texture_copy_source, &texture_size_box);
                }
                break;
            }

            m_temp_upload_buffers.push_back(UploadQueueKeepAlive{ m_upload_fence_value_when_done, upload_buffer });
        }

        id.is_loaded = true; // todo, only set this to true when the upload command buffer finished (when the fence value was reached)
        resource->name = name;
        resource->handle->SetName(std::wstring(name.begin(), name.end()).c_str());
        resource->subresource_handles = mip_handles;

        auto& mip_states = resource->subresource_states;
        mip_states.resize(mip_handles.size());
        std::fill(mip_states.begin(), mip_states.end(), D3D12_RESOURCE_STATE_COPY_DEST);

        return ResourceHandlePair{ id, resource };
    }

    ResourceHandlePair Device::load_mesh(const std::string& name, const uint64_t n_triangles, Triangle* tris) {
        return create_buffer(name, n_triangles * sizeof(Triangle), tris, ResourceUsage::non_pixel_shader_read);
    }

    void debug_scene_graph_nodes(SceneNode* node, int depth = 0) {
        for (int i = 0; i < depth; ++i) LOG(Debug, "    ");
        switch (node->type) {
            case gfx::SceneNodeType::Empty:
                LOG(Debug, "Node: %s", node->name.c_str());
                break;
            case gfx::SceneNodeType::Mesh:
                LOG(Debug, "Mesh: (Vertex buffer: %i) %s", node->mesh.vertex_buffer.id, node->name.c_str());
                break;
            case gfx::SceneNodeType::Light:
                LOG(Debug, "Light: %s", node->name.c_str());
                break;
        }
        for (auto& child : node->children) {
            debug_scene_graph_nodes(child.get(), depth + 1);
        }
    }

    ResourceHandlePair Device::create_buffer(const std::string& name, const size_t size, void* data, ResourceUsage usage) {
        // Create engine resource
        const auto resource = std::make_shared<Resource>(ResourceType::buffer);
        resource->usage = usage;
        resource->expect_buffer() = {
            .data = data,
            .size = size,
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
        if (usage == ResourceUsage::compute_write) resource_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        
        D3D12_HEAP_PROPERTIES heap_properties = {.Type = D3D12_HEAP_TYPE_DEFAULT};
        if      (usage == ResourceUsage::cpu_read_write) heap_properties.Type = D3D12_HEAP_TYPE_READBACK;
        else if (usage == ResourceUsage::cpu_writable)   heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;

        resource->current_state = D3D12_RESOURCE_STATE_COMMON;
        validate(device->CreateCommittedResource(
            &heap_properties,
            D3D12_HEAP_FLAG_NONE,
            &resource_desc,
            resource->current_state,
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

        auto id = m_heap_bindless->alloc_descriptor(ResourceType::buffer);
        const auto handle = m_heap_bindless->fetch_cpu_handle(id);
        device->CreateShaderResourceView(resource->handle.Get(), &srv_desc, handle);

        // If compute_write, also create an unordered access view
        if (usage == ResourceUsage::compute_write) {
            const D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc {
                .Format = DXGI_FORMAT_R32_TYPELESS,
                .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
                .Buffer = {
                    .FirstElement = 0,
                    .NumElements = static_cast<UINT>(size / 4),
                    .StructureByteStride = 0,
                    .CounterOffsetInBytes = 0,
                    .Flags = D3D12_BUFFER_UAV_FLAG_RAW
                }
            };

            auto uav_id = id;
            uav_id.id += 1;
            const auto uav_handle = m_heap_bindless->fetch_cpu_handle(uav_id);
            device->CreateUnorderedAccessView(resource->handle.Get(), nullptr, &uav_desc, uav_handle);
        }

        if (data && (usage == ResourceUsage::cpu_writable || usage == ResourceUsage::cpu_read_write)) {
            // Copy the buffer data to the GPU
            void* mapped_buffer = nullptr;
            D3D12_RANGE range = { 0, size };
            validate(resource->handle->Map(0, &range, &mapped_buffer));
            memcpy(mapped_buffer, data, size);
            resource->handle->Unmap(0, &range);
        }
        else if (data) {
            // Create upload buffer containing the data
            const auto upload_buffer_id = create_buffer("Upload buffer", size, data, ResourceUsage::cpu_writable);
            const auto& upload_buffer = upload_buffer_id.resource;
            queue_unload_bindless_resource(upload_buffer_id);

            // Upload the data to the destination buffer
            ++m_upload_fence_value_when_done;
            auto cmd = m_upload_queue->create_command_buffer(nullptr, m_upload_fence_value_when_done);
            transition_resource(cmd, resource, D3D12_RESOURCE_STATE_COPY_DEST);
            execute_resource_transitions(cmd);
            cmd->get()->CopyBufferRegion(resource->handle.Get(), 0, upload_buffer->handle.Get(), 0, size);
            transition_resource(cmd, resource, D3D12_RESOURCE_STATE_COMMON);
            execute_resource_transitions(cmd);
            m_temp_upload_buffers.push_back(UploadQueueKeepAlive{ m_upload_fence_value_when_done, upload_buffer });
        }

        auto name_str = std::wstring(name.begin(), name.end());
        resource->handle->SetName(name_str.c_str());
        resource->name = name;

        // Store the resource data in the device struct
        id.is_loaded = true;

        return ResourceHandlePair{ id, resource };
    }

    ResourceHandlePair Device::create_render_target(const std::string& name, uint32_t width, uint32_t height, PixelFormat pixel_format, std::optional<glm::vec4> clear_color, ResourceUsage extra_usage) {
        // Make texture resource
        const auto resource = std::make_shared<Resource>(ResourceType::texture);
        resource->usage = extra_usage;
        resource->expect_texture() = {
            .data = nullptr,
            .width = width,
            .height = height,
            .pixel_format = pixel_format,
            .clear_on_begin = clear_color.has_value(),
            .is_compute_render_target = true,
            .clear_color = clear_color.value_or(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)),
            .rtv_handle = ResourceHandle::none(),
            .dsv_handle = ResourceHandle::none(),
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
        resource_desc.Flags |= (extra_usage == ResourceUsage::compute_write) ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
        resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        const glm::vec4 clear_color_value = resource->expect_texture().clear_color;
        D3D12_CLEAR_VALUE clear_value = {
            .Format = resource_desc.Format,
            .Color = { clear_color_value.r, clear_color_value.g, clear_color_value.b, clear_color_value.a },
        };

        // Create resource
        D3D12_HEAP_PROPERTIES heap_properties = {};
        heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
        validate(device->CreateCommittedResource(
            &heap_properties,
            D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
            &resource_desc,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            &clear_value,
            IID_PPV_ARGS(&resource->handle)
        ));
        resource->current_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
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

        if (extra_usage == ResourceUsage::compute_write) {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {
                .Format = resource_desc.Format,
                .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
                .Texture2D = {
                    .MipSlice = 0,
                    .PlaneSlice = 0,
                }
            };
            auto uav_id = srv_id;
            uav_id.id += 1;
            auto uav_descriptor = m_heap_bindless->fetch_cpu_handle(uav_id);
            device->CreateUnorderedAccessView(resource->handle.Get(), nullptr, &uav_desc, uav_descriptor);
            uav_id.is_loaded = 1;
        }

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
        const auto resource = std::make_shared<Resource>(ResourceType::texture);
        resource->expect_texture() = {
            .data = nullptr,
            .width = width,
            .height = height,
            .pixel_format = pixel_format,
            .clear_on_begin = true,
            .is_compute_render_target = true,
            .clear_color = glm::vec4(clear_depth, 0.0f, 0.0f, 1.0f),
            .rtv_handle = ResourceHandle::none(),
            .dsv_handle = ResourceHandle::none(),
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
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clear_value,
            IID_PPV_ARGS(&resource->handle)
        ));
        resource->current_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
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
            std::optional<glm::vec4> clear_color{};
            if (texture.clear_on_begin) clear_color = texture.clear_color;
            m_heap_rtv->free_descriptor(resource->expect_texture().rtv_handle);
            handle = create_render_target(resource->name, width, height, texture.pixel_format, clear_color, resource->usage);
            return;
        }

        // If it's a depth target, create a new depth target
        if (texture.dsv_handle.type != (uint32_t)ResourceType::none) {
            m_heap_dsv->free_descriptor(resource->expect_texture().dsv_handle);
            handle = create_depth_target(resource->name, width, height, texture.pixel_format, texture.clear_color.r);
            return;
        }

        // If it's a render texture (compute shader render target), resize it
        if (texture.is_compute_render_target) {
            m_heap_bindless->free_descriptor(handle.handle);
            handle = load_texture(resource->name, width, height, 1, nullptr, texture.pixel_format, TextureType::tex_2d, resource->usage);
            return;
        }

        throw std::runtime_error("todo: when resizing regular textures, resize and copy the data to a new texture");
    }

    void Device::update_buffer(const ResourceHandlePair& buffer, const uint32_t offset, const uint32_t n_bytes, const void* data) {
        // todo: handle case for buffers that are not visible from cpu
        if ((buffer.resource->usage != ResourceUsage::cpu_read_write) && (buffer.resource->usage != ResourceUsage::cpu_writable)) {
            LOG(Error, "Write failed for \"%s\": buffer is not CPU writable!", buffer.resource->name.c_str());
            return;
        }

        if (data == nullptr) {
            LOG(Error, "Write failed for \"%s\": source data pointer is null!", buffer.resource->name.c_str());
            return;
        }

        if (buffer.resource->type != ResourceType::buffer) {
            LOG(Error, "Write failed for \"%s\": target resource is not a buffer!", buffer.resource->name.c_str());
            return;
        }

        if (offset + n_bytes - 1 > buffer.resource->expect_buffer().size) {
            LOG(Error, "Write failed for \"%s\": write range out of bounds! (range: %i - %i exceeds buffer size of %i bytes)", buffer.resource->name.c_str(), offset, offset + n_bytes, buffer.resource->expect_buffer().size);
            return;
        }

        char* mapped_buffer;
        const D3D12_RANGE write_range = { offset, offset + n_bytes };
        if (FAILED(buffer.resource->handle->Map(0, &write_range, (void**)&mapped_buffer))) {
            LOG(Error, "Write failed for \"%s\": failed to map buffer to CPU memory space!", buffer.resource->name.c_str());
            return;
        }
        memcpy(mapped_buffer + offset, data, n_bytes);
        buffer.resource->handle->Unmap(0, &write_range);
    }

    void Device::readback_buffer(const ResourceHandlePair& buffer, const uint32_t offset, const uint32_t n_bytes, void* destination) {
        if (buffer.resource->usage != ResourceUsage::cpu_read_write) {
            LOG(Error, "Readback failed for \"%s\": buffer is not CPU readable!", buffer.resource->name.c_str());
            return;
        }

        if (destination == nullptr) {
            LOG(Error, "Readback failed for \"%s\": destination pointer is null!", buffer.resource->name.c_str());
            return;
        }

        if (buffer.resource->type != ResourceType::buffer) {
            LOG(Error, "Readback failed for \"%s\": target resource is not a buffer!", buffer.resource->name.c_str());
            return;
        }

        if (offset + n_bytes - 1 > buffer.resource->expect_buffer().size) {
            LOG(Error, "Readback failed for \"%s\": max read range out of bounds! (range: %i - %i exceeds buffer size of %i bytes)", buffer.resource->name.c_str(), offset, offset + n_bytes - 1, buffer.resource->expect_buffer().size);
            return;
        }

        char* mapped_buffer;
        const D3D12_RANGE read_range = { offset, offset + n_bytes };
        if (FAILED(buffer.resource->handle->Map(0, &read_range, (void**)&mapped_buffer))) {
            LOG(Error, "Readback failed for \"%s\": failed to map buffer to CPU memory space!", buffer.resource->name.c_str());
            return;
        }
        memcpy(destination, mapped_buffer + offset, n_bytes);
        buffer.resource->handle->Unmap(0, nullptr);
    }

    void Device::queue_unload_bindless_resource(ResourceHandlePair resource) {
        int fence_value = m_swapchain->current_frame_index() + 3;
        m_resources_to_unload.push_back({ resource, fence_value });
    }

    D3D12_RESOURCE_STATES resource_usage_to_dx12_state(ResourceUsage usage) {
        switch (usage) {
            case ResourceUsage::none: return D3D12_RESOURCE_STATE_COMMON;
            case ResourceUsage::read: return D3D12_RESOURCE_STATE_GENERIC_READ;
            case ResourceUsage::compute_write: return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            case ResourceUsage::render_target: return D3D12_RESOURCE_STATE_RENDER_TARGET;
            case ResourceUsage::depth_target: return D3D12_RESOURCE_STATE_DEPTH_WRITE;
            case ResourceUsage::pixel_shader_read: return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            case ResourceUsage::non_pixel_shader_read: return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        }
        return D3D12_RESOURCE_STATE_COMMON;
    }

    void Device::use_resource(const ResourceHandlePair &resource, const ResourceUsage usage) {
        transition_resource(m_curr_pass_cmd, resource.resource, resource_usage_to_dx12_state(usage));
        execute_resource_transitions(m_curr_pass_cmd);
    }

    void Device::use_resources(const std::initializer_list<ResourceTransitionInfo>& resources) {
        for (auto& [resource, usage, subresource] : resources) {
            if (resource.resource != nullptr) transition_resource(m_curr_pass_cmd, resource.resource, resource_usage_to_dx12_state(usage), subresource);
        }
        execute_resource_transitions(m_curr_pass_cmd);
    }

    void Device::transition_resource(std::shared_ptr<CommandBuffer> cmd, std::shared_ptr<Resource> resource, D3D12_RESOURCE_STATES new_state, uint32_t subresource) {
        auto current_state = (subresource == (uint32_t)-1 || subresource == 0) ? (resource->current_state) : (resource->subresource_states[subresource - 1]);
        if (current_state == new_state) return;

        if (m_curr_pipeline_is_async) {
            m_temp_upload_buffers.push_back(UploadQueueKeepAlive{ m_upload_fence_value_when_done, resource });
        }

        if (resource->current_state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
            m_resource_barriers.emplace_back(D3D12_RESOURCE_BARRIER {
                .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
                .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                .UAV = {
                    .pResource = resource->handle.Get(),
                },
            });
        }

        m_resource_barriers.emplace_back(D3D12_RESOURCE_BARRIER {
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .Transition = {
                .pResource = resource->handle.Get(),
                .Subresource = subresource,
                .StateBefore = current_state,
                .StateAfter = new_state,
            },
        });

        if (new_state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
            m_resource_barriers.emplace_back(D3D12_RESOURCE_BARRIER {
                .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
                .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                .UAV = {
                    .pResource = resource->handle.Get(),
                },
            });
        }
        
        if (subresource == 0) resource->current_state = new_state;
        else if (subresource != (uint32_t)-1) resource->subresource_states[subresource - 1] = new_state;
        else {
            resource->current_state = new_state;
            for (auto& state : resource->subresource_states) {
                state = new_state;
            }
        }
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
            LOG(Debug, "Display %i: %ix%i @ %ix%i", 0, w, h, x, y);
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
            m_resources_to_unload.pop_front();
        }

        while (!m_temp_upload_buffers.empty()) {
            // Get the next entry in the unload queue
            auto& upload_data = m_temp_upload_buffers.front();

            // If it's still potentially being used, end the loop here, because all subsequent queue entries have
            // the equal or higher desired fence values
            if (m_upload_queue_completion_fence->reached_value(upload_data.upload_queue_fence_value) == false) break;

            // Destroy, Erase, Improve (memory usage) - good Meshuggah album btw, go listen to it
            m_temp_upload_buffers.pop_front();
        }
    }
    
    void Device::execute_resource_transitions(std::shared_ptr<CommandBuffer> cmd) {
        if (m_resource_barriers.empty()) return;

        cmd->get()->ResourceBarrier((UINT)m_resource_barriers.size(), m_resource_barriers.data());
        m_resource_barriers.clear();
    }
    
    ID3D12Device5* Device::device5() {
        ID3D12Device5* device5;
        HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&device5));
        if (FAILED(hr)) {
            LOG(Error, "Failed to get ID3D12Device5* interface");
            return nullptr;
        }
        return device5;
    }
}
