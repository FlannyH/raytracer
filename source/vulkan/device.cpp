#include "device.h"

#include <array>
#include "command_queue.h"
#include "descriptor_heap.h"
#include "../input.h"

namespace gfx::vk {
    Device::Device(const int width, const int height, const bool debug_layer_enabled, const bool gpu_profiling_enabled) {
        const std::array<const char*, 3> device_extensions_to_enable = {
            "VK_KHR_swapchain",
            "VK_KHR_buffer_device_address",
            "VK_EXT_descriptor_indexing",
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
            const std::array<const char*, 1> debug_layers_to_enable = {
                // todo: take a proper look at validation layers
                "VK_LAYER_KHRONOS_validation",
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

#ifdef _DEBUG
        LOG(Debug, "Available devices:");
        for (auto& physical_device : physical_devices) {
            VkPhysicalDeviceProperties device_properties;
            vkGetPhysicalDeviceProperties(physical_device, &device_properties);
            const char* device_type_names[] = { "", " (Integrated GPU)", " (Discrete GPU)", " (Virtual GPU)", "(CPU)" };
            LOG(Debug, "\t%s%s", device_properties.deviceName, device_type_names[(size_t)device_properties.deviceType]);
        }
#endif

        for (auto& physical_device : physical_devices) {
            VkPhysicalDeviceProperties device_properties;
            vkGetPhysicalDeviceProperties(physical_device, &device_properties);

            if (device_properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) continue;
            
            LOG(Info, "Using device \"%s\"", device_properties.deviceName);

            uint32_t n_queue_family_properties = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &n_queue_family_properties, nullptr);

            std::vector<VkQueueFamilyProperties> queue_family_properties(n_queue_family_properties);
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &n_queue_family_properties, queue_family_properties.data());

            for (int i = 0; i < n_queue_family_properties; ++i) {
                const auto& family = queue_family_properties[i];
                if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) m_indices.graphics_family = i;
                if (family.queueFlags & VK_QUEUE_COMPUTE_BIT)  m_indices.compute_family =  i;
            }

            const float queue_priority_graphics = 1.0f;
            const float queue_priority_compute = 0.8f;

            std::array<VkDeviceQueueCreateInfo, 2> device_queue_create_info = {{
                VkDeviceQueueCreateInfo { 
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = m_indices.graphics_family.value(),
                    .queueCount = 1,
                    .pQueuePriorities = &queue_priority_graphics,
                },
                VkDeviceQueueCreateInfo {        
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = m_indices.compute_family.value(),
                    .queueCount = 1,
                    .pQueuePriorities = &queue_priority_compute,
                }
            }};

            VkDeviceCreateInfo device_create_info{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
            device_create_info.queueCreateInfoCount = device_queue_create_info.size();
            device_create_info.pQueueCreateInfos = device_queue_create_info.data();
            device_create_info.enabledExtensionCount = device_extensions_to_enable.size();
            device_create_info.ppEnabledExtensionNames = device_extensions_to_enable.data();

            m_physical_device = physical_device;

            const auto result = vkCreateDevice(physical_device, &device_create_info, nullptr, &device);
            if (result != VK_SUCCESS) {
                LOG(Error, "Failed to create Vulkan device on physical device \"%s\": error %i", device_properties.deviceName, result);
            }

            input::init(m_window_glfw);

            break;
        }

        m_queue_graphics = std::make_shared<CommandQueue>(*this, CommandBufferType::graphics);
        m_queue_compute = std::make_shared<CommandQueue>(*this, CommandBufferType::compute);
        m_desc_heap = std::make_shared<DescriptorHeap>(*this, 100'000); // todo: unhardcode this
    }
    
    Device::~Device() {
        vkDestroyDevice(device, nullptr);
    }

    void Device::resize_window(const int width, const int height) const {
        TODO();
    }

    void Device::get_window_size(int& width, int& height) const {
        TODO();
    }

    void Device::begin_frame() {
        TODO();
    }

    void Device::end_frame() {
        TODO();
        glfwPollEvents();
        glfwSwapBuffers(m_window_glfw);
    }

    void Device::set_graphics_root_constants(const std::vector<uint32_t>& constants) {
        TODO();
    }

    void Device::set_compute_root_constants(const std::vector<uint32_t>& constants) {
        TODO();
    }

    int Device::frame_index() {
        TODO();
        return -1;
    }

    bool Device::supports(RendererFeature feature) {
        TODO();
        return false;
    }

    void Device::end_raster_pass() {
        TODO();
    }

    void Device::end_compute_pass() {
        TODO();
    }

    void Device::dispatch_threadgroups(uint32_t x, uint32_t y, uint32_t z) {
        TODO();
    }

    void Device::draw_vertices(uint32_t n_vertices) {
        TODO();
    }

    ResourceHandlePair Device::load_texture(const std::string& name, uint32_t width, uint32_t height, uint32_t depth, void* data, PixelFormat pixel_format, TextureType type, ResourceUsage usage, int max_mip_levels, int min_resolution) {
        TODO();
        return {};
    }

    ResourceHandlePair Device::load_mesh(const std::string& name, const uint64_t n_triangles, Triangle* tris) {
        TODO();
        return {};
    }

    VkImageLayout resource_usage_to_vk_image_layout(ResourceUsage usage) {
        switch (usage) {
            case ResourceUsage::none: return VK_IMAGE_LAYOUT_GENERAL;
            case ResourceUsage::read: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case ResourceUsage::compute_write: return VK_IMAGE_LAYOUT_GENERAL;
            case ResourceUsage::render_target: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            case ResourceUsage::depth_target: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            case ResourceUsage::pixel_shader_read: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case ResourceUsage::non_pixel_shader_read: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case ResourceUsage::acceleration_structure: return VK_IMAGE_LAYOUT_GENERAL; // General for acceleration structures
        }
        return VK_IMAGE_LAYOUT_GENERAL; // Default fallback
    }

    VkBufferUsageFlags resource_usage_to_vk_buffer_usage(ResourceUsage usage) {
        switch (usage) {
            case ResourceUsage::none:
            case ResourceUsage::read:
            case ResourceUsage::pixel_shader_read:
            case ResourceUsage::non_pixel_shader_read: 
            case ResourceUsage::cpu_read_write:
            case ResourceUsage::cpu_writable: return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; // Read-only in shaders
            case ResourceUsage::compute_write: return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            case ResourceUsage::render_target: return 0; // Not applicable for buffers
            case ResourceUsage::depth_target: return 0; // Not applicable for buffers
            case ResourceUsage::acceleration_structure: return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR; // For acceleration structures
        }
        return 0; // Default fallback
    }

    uint32_t find_memory_type(VkPhysicalDevice& physical_device, uint32_t type_filter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties mem_properties;
        vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

        for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
            if (type_filter & (1 << i) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        LOG(Warning, "Failed to find suitable memory type");
        return 0;
    }

    ResourceHandlePair Device::create_buffer(const std::string& name, const size_t size, void* data, ResourceUsage usage) {
        // Create buffer
        // todo: destroy `buffer` when destroying the resource
        VkBuffer buffer;
        VkBufferCreateInfo buffer_create_info { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        buffer_create_info.size = size;
        buffer_create_info.usage = resource_usage_to_vk_buffer_usage(usage);
        
        if (vkCreateBuffer(device, &buffer_create_info, nullptr, &buffer) != VK_SUCCESS) {
        buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            LOG(Error, "Failed to create buffer \"%s\"", name.c_str());
            return {};
        }

        // Allocate buffer memory
        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(device, buffer, &memory_requirements);

        VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT; // gpu-only by default
        if (usage == (ResourceUsage::cpu_read_write)) flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT; // can read and write from cpu side and gpu side
        if (usage == (ResourceUsage::cpu_writable)) flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT; // can only write from cpu side and both read and write on gpu side

        VkMemoryAllocateInfo memory_allocate_info { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        memory_allocate_info.allocationSize = memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = find_memory_type(m_physical_device, memory_requirements.memoryTypeBits, flags);

        // todo: deallocate this when destroying the resource
        VkDeviceMemory device_memory;
        if (vkAllocateMemory(device, &memory_allocate_info, nullptr, &device_memory) != VK_SUCCESS) {
            LOG(Error, "Failed to allocate memory for buffer \"%s\"", name.c_str());
            return {};
        }
        if (vkBindBufferMemory(device, buffer, device_memory, 0) != VK_SUCCESS) {
            LOG(Error, "Failed to bind memory for buffer \"%s\"", name.c_str());
            return {};
        }

        // Populate buffer
        if (data != nullptr && usage == (ResourceUsage::cpu_writable) || usage == (ResourceUsage::cpu_read_write)) {
            // todo: do i need to flush this memory?
            void* mapped_buffer;
            const auto result = vkMapMemory(device, device_memory, 0, size, 0, &mapped_buffer);
            if (result == VK_SUCCESS) {
                memcpy(mapped_buffer, data, size);
                vkUnmapMemory(device, device_memory);
            }
            else {
                LOG(Error, "Failed to map buffer memory for \"%s\": error %i", name.c_str(), result);
            }
        }
        else if (data != nullptr) {
            // Create upload buffer containing the data
            const auto upload_buffer_id = create_buffer(name + "(upload buffer)", size, data, ResourceUsage::cpu_writable);
            const auto& upload_buffer = upload_buffer_id.resource;
            // todo: unload this buffer later

            // Upload the data to the destination buffer
            ++m_upload_fence_value_when_done;
            auto cmd = m_queue_compute->create_command_buffer(device, nullptr, m_upload_fence_value_when_done);
            
            VkBufferCopy region = {};
            region.srcOffset = 0;
            region.dstOffset = 0;
            region.size = size;

            vkCmdCopyBuffer(cmd, upload_buffer->expect_buffer().vk_buffer, buffer, 1, &region);
        }

        // Create engine resource
        const auto resource = std::make_shared<Resource>(ResourceType::buffer);
        resource->name = name;
        resource->usage = usage;
        resource->expect_buffer() = {
            .data = data,
            .size = size,
            .vk_buffer = buffer,
        };

        // todo: set the name on the vulkan buffer object as well
        auto id = m_desc_heap->alloc_descriptor(ResourceType::buffer);
        m_desc_heap->write_buffer_descriptor(*this, id, buffer, 0, size);

        return ResourceHandlePair{ id, resource };
        return {};
    }

    ResourceHandlePair Device::create_render_target(const std::string& name, uint32_t width, uint32_t height, PixelFormat pixel_format, std::optional<glm::vec4> clear_color, ResourceUsage extra_usage) {
        TODO();
        return {};
    }

    ResourceHandlePair Device::create_depth_target(const std::string& name, uint32_t width, uint32_t height, PixelFormat pixel_format, float clear_depth) {
        TODO();
        return {};
    }

    void Device::resize_texture(ResourceHandlePair& handle, const uint32_t width, const uint32_t height) {
        TODO();
    }

    void Device::update_buffer(const ResourceHandlePair& buffer, const uint32_t offset, const uint32_t n_bytes, const void* data) {
        TODO();
    }

    void Device::readback_buffer(const ResourceHandlePair& buffer, const uint32_t offset, const uint32_t n_bytes, void* destination) {
        TODO();
    }

    void Device::queue_unload_bindless_resource(ResourceHandlePair resource) {
        TODO();
    }

    void Device::use_resource(const ResourceHandlePair &resource, const ResourceUsage usage) {
        TODO();
    }

    void Device::use_resources(const std::initializer_list<ResourceTransitionInfo>& resources) {
        TODO();
    }

    ResourceHandlePair Device::create_acceleration_structure(const std::string& name, const size_t size) {
        TODO();
        return {};
    }

    ResourceHandlePair Device::create_blas(const std::string& name, const ResourceHandlePair& position_buffer, const ResourceHandlePair& index_buffer, const uint32_t vertex_count, const uint32_t index_count) {
        TODO();
        return {};
    }

    ResourceHandlePair Device::create_tlas(const std::string& name, const std::vector<RaytracingInstance>& instances) {
        TODO();
        return {};
    }

    bool Device::should_stay_open() {
        return !glfwWindowShouldClose(m_window_glfw);
    }

    void Device::set_full_screen(bool full_screen) {
        TODO();
    }
}