#include "../device.h"

#include <array>
#include "../input.h"

namespace gfx {
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphics_family;
        std::optional<uint32_t> compute_family;
    };

    Device::Device(const int width, const int height, const bool debug_layer_enabled, const bool gpu_profiling_enabled) {
        const std::array<const char*, 1> device_extensions_to_enable = {
            "VK_KHR_swapchain"
        };

        glfwInit();
        
        uint32_t n_instance_extensions_to_enable = 0;
        const char** instance_extensions_to_enable = glfwGetRequiredInstanceExtensions(&n_instance_extensions_to_enable);

        if (!glfwVulkanSupported()) {
            LOG(Fatal, "GLFW reports Vulkan is not supported on this system.");
            return;
        }

        for (uint32_t i = 0; i < n_instance_extensions_to_enable; ++i) {
            LOG(Debug, "Required instance extension: %s", instance_extensions_to_enable[i]);
        }

        VkInstanceCreateInfo instance_create_info{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        instance_create_info.enabledExtensionCount = n_instance_extensions_to_enable;
        instance_create_info.ppEnabledExtensionNames = instance_extensions_to_enable;

        if (debug_layer_enabled) {
            const std::array<const char*, 0> debug_layers_to_enable = {
                // todo: take a proper look at validation layers
                // "VK_LAYER_KHRONOS_validation",
            };
            instance_create_info.enabledLayerCount = debug_layers_to_enable.size();
            instance_create_info.ppEnabledLayerNames = debug_layers_to_enable.data();
        }
        else {
            instance_create_info.enabledLayerCount = 0;
            instance_create_info.ppEnabledLayerNames = nullptr;
        }

        VkInstance instance;
        VkResult result;
        result = vkCreateInstance(&instance_create_info, nullptr, &instance);
        if (result != VK_SUCCESS) {
            LOG(Fatal, "Failed to create Vulkan instance");
            LOG(Debug, "VkResult: 0x%08x (%i)\n", result, result);
            return;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        m_window_glfw = glfwCreateWindow(width, height, "Ray Tracer (Vulkan)", nullptr, nullptr);

        VkSurfaceKHR surface;
        result = glfwCreateWindowSurface(instance, m_window_glfw, NULL, &surface);
        if (result != VK_SUCCESS) {
            LOG(Fatal, "Failed to create window surface");
            LOG(Debug, "VkResult: 0x%08x (%i)\n", result, result);
            return;
        }

        uint32_t n_physical_devices = 0;
        vkEnumeratePhysicalDevices(instance, &n_physical_devices, nullptr);

        if (n_physical_devices == 0) {
            LOG(Fatal, "Failed to find GPU with Vulkan support");
            return;
        }

        std::vector<VkPhysicalDevice> physical_devices(n_physical_devices);
        vkEnumeratePhysicalDevices(instance, &n_physical_devices, physical_devices.data());

        VkDevice device;

        for (auto& physical_device : physical_devices) {
            VkPhysicalDeviceProperties device_properties;
            vkGetPhysicalDeviceProperties(physical_device, &device_properties);

            if (device_properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) continue;

            uint32_t n_queue_family_properties = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &n_queue_family_properties, nullptr);

            std::vector<VkQueueFamilyProperties> queue_family_properties(n_queue_family_properties);
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &n_queue_family_properties, queue_family_properties.data());

            QueueFamilyIndices indices{};
            for (int i = 0; i < n_queue_family_properties; ++i) {
                const auto& family = queue_family_properties[i];
                if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphics_family = i;
                if (family.queueFlags & VK_QUEUE_COMPUTE_BIT)  indices.compute_family =  i;
            }

            const float queue_priority_graphics = 1.0f;
            const float queue_priority_compute = 0.8f;

            std::array<VkDeviceQueueCreateInfo, 2> device_queue_create_info = {{
                VkDeviceQueueCreateInfo { 
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = indices.graphics_family.value(),
                    .queueCount = 1,
                    .pQueuePriorities = &queue_priority_graphics,
                },
                VkDeviceQueueCreateInfo {        
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = indices.compute_family.value(),
                    .queueCount = 1,
                    .pQueuePriorities = &queue_priority_compute,
                }
            }};

            VkDeviceCreateInfo device_create_info{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
            device_create_info.queueCreateInfoCount = device_queue_create_info.size();
            device_create_info.pQueueCreateInfos = device_queue_create_info.data();
            device_create_info.enabledExtensionCount = device_extensions_to_enable.size();
            device_create_info.ppEnabledExtensionNames = device_extensions_to_enable.data();

            vkCreateDevice(physical_device, &device_create_info, nullptr, &device);

            input::init(m_window_glfw);

            break;
        }
    }
    
    Device::~Device() {
    }

    void Device::resize_window(const int width, const int height) const {
    }

    void Device::get_window_size(int& width, int& height) const {
    }

    std::shared_ptr<Pipeline> Device::create_raster_pipeline(const std::string& name, const std::string& vertex_shader_path, const std::string& pixel_shader_path, const std::initializer_list<ResourceHandlePair> render_targets, const ResourceHandlePair depth_target) {
        return nullptr;
    }

    std::shared_ptr<Pipeline> Device::create_compute_pipeline(const std::string& name, const std::string& compute_shader_path) {
        return nullptr;
    }

    void Device::begin_frame() {
    }

    void Device::end_frame() {
        glfwPollEvents();
        glfwSwapBuffers(m_window_glfw);
    }

    void Device::set_graphics_root_constants(const std::vector<uint32_t>& constants) {
    }

    void Device::set_compute_root_constants(const std::vector<uint32_t>& constants) {
    }

    int Device::frame_index() {
        return -1;
    }

    bool Device::supports(RendererFeature feature) {
        return false;
    }

    void Device::begin_raster_pass(std::shared_ptr<Pipeline> pipeline, RasterPassInfo&& render_pass_info) {
    }

    void Device::end_raster_pass() {
    }

    void Device::begin_compute_pass(std::shared_ptr<Pipeline> pipeline, bool async) {
    }

    void Device::end_compute_pass() {
    }

    void Device::dispatch_threadgroups(uint32_t x, uint32_t y, uint32_t z) {
    }

    void Device::draw_vertices(uint32_t n_vertices) {
    }

    ResourceHandlePair Device::load_texture(const std::string& name, uint32_t width, uint32_t height, uint32_t depth, void* data, PixelFormat pixel_format, TextureType type, ResourceUsage usage, int max_mip_levels, int min_resolution) {
        return {};
    }

    ResourceHandlePair Device::load_mesh(const std::string& name, const uint64_t n_triangles, Triangle* tris) {
        return {};
    }

    void debug_scene_graph_nodes(SceneNode* node, int depth = 0) {
    }

    ResourceHandlePair Device::create_buffer(const std::string& name, const size_t size, void* data, ResourceUsage usage) {
        return {};
    }

    ResourceHandlePair Device::create_render_target(const std::string& name, uint32_t width, uint32_t height, PixelFormat pixel_format, std::optional<glm::vec4> clear_color, ResourceUsage extra_usage) {
        return {};
    }

    ResourceHandlePair Device::create_depth_target(const std::string& name, uint32_t width, uint32_t height, PixelFormat pixel_format, float clear_depth) {
        return {};
    }

    void Device::resize_texture(ResourceHandlePair& handle, const uint32_t width, const uint32_t height) {
    }

    void Device::update_buffer(const ResourceHandlePair& buffer, const uint32_t offset, const uint32_t n_bytes, const void* data) {
    }

    void Device::readback_buffer(const ResourceHandlePair& buffer, const uint32_t offset, const uint32_t n_bytes, void* destination) {
    }

    void Device::queue_unload_bindless_resource(ResourceHandlePair resource) {
    }

    D3D12_RESOURCE_STATES resource_usage_to_dx12_state(ResourceUsage usage) {
        return {};
    }

    void Device::use_resource(const ResourceHandlePair &resource, const ResourceUsage usage) {
    }

    void Device::use_resources(const std::initializer_list<ResourceTransitionInfo>& resources) {
    }

    ResourceHandlePair Device::create_acceleration_structure(const std::string& name, const size_t size) {
        return {};
    }

    ResourceHandlePair Device::create_blas(const std::string& name, const ResourceHandlePair& position_buffer, const ResourceHandlePair& index_buffer, const uint32_t vertex_count, const uint32_t index_count) {
        return {};
    }

    ResourceHandlePair Device::create_tlas(const std::string& name, const std::vector<RaytracingInstance>& instances) {
        return {};
    }

    void Device::transition_resource(std::shared_ptr<CommandBuffer> cmd, std::shared_ptr<Resource> resource, D3D12_RESOURCE_STATES new_state, uint32_t subresource) {
    }

    bool Device::should_stay_open() {
        return !glfwWindowShouldClose(m_window_glfw);
    }

    void Device::set_full_screen(bool full_screen) {
    }

    int Device::find_dominant_monitor() {
        return -1;
    }

    void Device::clean_up_old_resources() {
    }
    
    void Device::execute_resource_transitions(std::shared_ptr<CommandBuffer> cmd) {
    }
    
    ID3D12Device5* Device::device5() {
        return nullptr;
    }
}