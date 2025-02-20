#include "device.h"

#include <array>
#include <cstring>
#include "command_queue.h"
#include "descriptor_heap.h"
#include "helpers.h"
#include "../input.h"

namespace gfx::vk {
    Device::Device(const int width, const int height, const bool debug_layer_enabled, const bool gpu_profiling_enabled) {
        const std::array<const char*, 5> device_extensions_to_enable = {
            "VK_KHR_swapchain",
            "VK_KHR_buffer_device_address",
            "VK_EXT_descriptor_indexing",
            "VK_KHR_acceleration_structure",
            "VK_KHR_deferred_host_operations",
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

            VkPhysicalDeviceFeatures physical_device_features{};
            physical_device_features.samplerAnisotropy = true;

            VkDeviceCreateInfo device_create_info{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
            device_create_info.queueCreateInfoCount = device_queue_create_info.size();
            device_create_info.pQueueCreateInfos = device_queue_create_info.data();
            device_create_info.enabledExtensionCount = device_extensions_to_enable.size();
            device_create_info.ppEnabledExtensionNames = device_extensions_to_enable.data();
            device_create_info.pEnabledFeatures = &physical_device_features;

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
        
        // Regular texture sampler
        VkSamplerCreateInfo create_info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        create_info.magFilter = VK_FILTER_LINEAR;
        create_info.minFilter = VK_FILTER_LINEAR;
        create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.mipLodBias = 0.0f;
        create_info.anisotropyEnable = true;
        create_info.maxAnisotropy = 16.0f;
        create_info.compareEnable = false;
        create_info.minLod = 0.0f,
        create_info.maxLod = 100000.0f;
        create_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        create_info.unnormalizedCoordinates = false;
        vkCreateSampler(device, &create_info, nullptr, &m_samplers[0]);

        // Lookup texture (clamp)
        create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.anisotropyEnable = false;
        vkCreateSampler(device, &create_info, nullptr, &m_samplers[1]);

        // Cubemap
        // todo: lookup and cubemap are the exact same, maybe merge?
        vkCreateSampler(device, &create_info, nullptr, &m_samplers[2]);
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

    PipelineHandle Device::create_raster_pipeline(const std::string& name, const std::string& vertex_shader_path, const std::string& pixel_shader_path, const std::initializer_list<ResourceHandlePair> render_targets, const ResourceHandlePair depth_target) {
        TODO();
        return PIPELINE_NULL;
    }

    void Device::begin_raster_pass(PipelineHandle pipeline, RasterPassInfo&& render_pass_info) {
        TODO();
    }

    void Device::end_raster_pass() {
        TODO();
    }

    PipelineHandle Device::create_compute_pipeline(const std::string& name, const std::string& compute_shader_path) {
        return PipelineHandle();
    }

    void Device::begin_compute_pass(PipelineHandle pipeline, bool async) {
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
        if (usage != ResourceUsage::cpu_read_write && usage != ResourceUsage::cpu_writable && usage != ResourceUsage::copy_source) {
            buffer_create_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }
        
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
        if (usage == (ResourceUsage::copy_source)) flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT; // can only write from cpu side and both read and write on gpu side

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
        if (data != nullptr && usage == (ResourceUsage::cpu_writable) || usage == (ResourceUsage::cpu_read_write) || usage == (ResourceUsage::copy_source)) {
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
            const auto upload_buffer_id = create_buffer(name + "(upload buffer)", size, data, ResourceUsage::copy_source);
            const auto& upload_buffer = upload_buffer_id.resource;
            // todo: unload this buffer later

            // Upload the data to the destination buffer
            ++m_upload_fence_value_when_done;
            auto cmd = m_queue_compute->create_command_buffer(device, nullptr, m_upload_fence_value_when_done);
            
            VkBufferCopy region = {};
            region.srcOffset = 0;
            region.dstOffset = 0;
            region.size = size;

            // todo: vkCmdCopyBuffer(cmd, upload_buffer->expect_buffer().vk_buffer, buffer, 1, &region);
        }

        // Create engine resource
        const auto resource = std::make_shared<Resource>(ResourceType::buffer);
        resource->name = name;
        resource->usage = usage;
        resource->expect_buffer() = {
            .data = data,
            .size = size,
            // todo: .vk_buffer = buffer,
        };

        // todo: set the name on the vulkan buffer object as well
        auto id = m_desc_heap->alloc_descriptor(ResourceType::buffer);
        if (usage != ResourceUsage::copy_source) {
            m_desc_heap->write_buffer_descriptor(*this, id, buffer, 0, size);
        }

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

        VkImageCreateInfo image_create_info { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        image_create_info.flags = 0;
        image_create_info.imageType = VK_IMAGE_TYPE_2D;
        image_create_info.format = pixel_format_to_vk(pixel_format);
        image_create_info.extent = { width, height, 1 };
        image_create_info.mipLevels = 1;
        image_create_info.arrayLayers = 1;
        image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_create_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_create_info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

        VkImage image;
        if (vkCreateImage(device, &image_create_info, nullptr, &image) != VK_SUCCESS) {
            LOG(Error, "Failed to create image for render target \"%s\"", name.c_str());
        }
        
        VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT; // gpu-only by default
        if (extra_usage == (ResourceUsage::cpu_read_write)) flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        if (extra_usage == (ResourceUsage::cpu_writable)) flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        
        VkMemoryRequirements memory_requirements;
        vkGetImageMemoryRequirements(device, image, &memory_requirements);

        VkMemoryAllocateInfo memory_allocate_info { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        memory_allocate_info.allocationSize = memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = find_memory_type(m_physical_device, memory_requirements.memoryTypeBits, flags);

        // todo: deallocate this when destroying the resource
        VkDeviceMemory device_memory;
        if (vkAllocateMemory(device, &memory_allocate_info, nullptr, &device_memory) != VK_SUCCESS) {
            LOG(Error, "Failed to allocate memory for buffer \"%s\"", name.c_str());
            return {};
        }
        if (vkBindImageMemory(device, image, device_memory, 0) != VK_SUCCESS) {
            LOG(Error, "Failed to bind memory for buffer \"%s\"", name.c_str());
            return {};
        }

        auto id = m_desc_heap->alloc_descriptor(ResourceType::texture);

        auto& resource_info = fetch_resource_info(id);
        resource_info.access_mask = VK_ACCESS_NONE;
        resource_info.image_layout = image_create_info.initialLayout;
        resource_info.queue_family_index = VK_QUEUE_FAMILY_IGNORED;

        VkCommandBuffer cmd = m_queue_compute->create_command_buffer(device, nullptr, ++m_upload_fence_value_when_done);
        transition_resource(cmd, 
            ResourceHandlePair{id, resource}, 
            ResourceInfo { 
                .image_layout = extra_usage == ResourceUsage::compute_write? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            }, 
            VkImageSubresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        );

        VkImageViewCreateInfo image_view_create_info { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        image_view_create_info.image = image;
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format = pixel_format_to_vk(pixel_format);
        image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_create_info.subresourceRange.baseMipLevel = 0;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.layerCount = 1;

        VkImageView image_view;
        if (vkCreateImageView(device, &image_view_create_info, nullptr, &image_view) != VK_SUCCESS) {
            LOG(Error, "Failed to create image view for render target \"%s\"", name.c_str());
        }

        // todo: resource->expect_texture().vk_image = image;
        // todo: resource->expect_texture().vk_image_view = image_view;

        m_desc_heap->write_texture_descriptor(*this, id, image_create_info.initialLayout, image_view);

        return ResourceHandlePair{ id, resource };
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

    void Device::use_resource(const ResourceHandle handle, const ResourceUsage usage) {
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

    void Device::transition_resource(VkCommandBuffer cmd, ResourceHandlePair resource, ResourceInfo&& new_state, VkImageSubresourceRange subresource_range) {
        const ResourceInfo& resource_info = fetch_resource_info(resource.handle);

    	if (new_state.image_layout != VK_IMAGE_LAYOUT_UNDEFINED) {
            m_queued_image_memory_barriers.emplace_back(VkImageMemoryBarrier {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask = resource_info.access_mask,
                .dstAccessMask = (new_state.access_mask == VK_ACCESS_FLAG_BITS_MAX_ENUM)?
                    resource_info.access_mask
                :
                    new_state.access_mask
                ,
                .oldLayout = resource_info.image_layout,
                .newLayout = new_state.image_layout,
                .srcQueueFamilyIndex = resource_info.queue_family_index,
                .dstQueueFamilyIndex = (new_state.queue_family_index == 0xFFFFFFFF)?
                    resource_info.queue_family_index
                :
                    new_state.queue_family_index
                ,
                .image = fetch_resource_info(resource.handle).image,
                .subresourceRange = subresource_range
            });
        }
        else {
            m_queued_buffer_memory_barriers.emplace_back(VkBufferMemoryBarrier {
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .srcAccessMask = resource_info.access_mask,
                .dstAccessMask = (new_state.access_mask == VK_ACCESS_FLAG_BITS_MAX_ENUM)?
                    resource_info.access_mask
                :
                    new_state.access_mask
                ,
                .srcQueueFamilyIndex = resource_info.queue_family_index,
                .dstQueueFamilyIndex = (new_state.queue_family_index == 0xFFFFFFFF)?
                    resource_info.queue_family_index
                :
                    new_state.queue_family_index
                ,
                .buffer = fetch_resource_info(resource.handle).buffer,
                .offset = 0,
                .size = resource.resource->expect_buffer().size
            });
        }
    }

    void Device::execute_resource_transitions(VkCommandBuffer cmd, VkPipelineStageFlags source, VkPipelineStageFlags destination)  {
        vkCmdPipelineBarrier(cmd, source, destination, 0,
            0, nullptr,
            m_queued_buffer_memory_barriers.size(),
            m_queued_buffer_memory_barriers.data(),
            m_queued_image_memory_barriers.size(),
            m_queued_image_memory_barriers.data()
        );
        
        m_queued_buffer_memory_barriers.clear();
        m_queued_image_memory_barriers.clear();
    }

    bool Device::should_stay_open() {
        return !glfwWindowShouldClose(m_window_glfw);
    }

    void Device::set_full_screen(bool full_screen) {
        TODO();
    }
    
    ResourceInfo& Device::fetch_resource_info(ResourceHandle handle) {
        // Always use the ID of the first handle
        return m_resource_info[handle.id & ~1];
    }
}