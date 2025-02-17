#include "descriptor_heap.h"

namespace gfx::vk {    
    DescriptorHeap::DescriptorHeap(const Device& device, uint32_t n_descriptors) {
        // Create descriptor set layout with descriptor binding flags PARTIALLY_BOUND, VARIABLE_DESCRIPTOR_COUNT, UPDATE_AFTER_BIND
        const std::array<VkDescriptorSetLayoutBinding, 4> desc_set_layout_binding = {{
            { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, n_descriptors, VK_SHADER_STAGE_ALL },
            { 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, n_descriptors, VK_SHADER_STAGE_ALL },
            { 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, n_descriptors, VK_SHADER_STAGE_ALL },
            { 3, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, n_descriptors, VK_SHADER_STAGE_ALL }
        }};

        const std::array<VkDescriptorBindingFlags, 4> flags = {{
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
        }};

        VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
        binding_flags_create_info.bindingCount = flags.size();
        binding_flags_create_info.pBindingFlags = flags.data();

        VkDescriptorSetLayoutCreateInfo desc_set_create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        desc_set_create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        desc_set_create_info.bindingCount = desc_set_layout_binding.size();
        desc_set_create_info.pBindings = desc_set_layout_binding.data();

        VkDescriptorSetLayout desc_set_layout;
        if (vkCreateDescriptorSetLayout(device.device, &desc_set_create_info, nullptr, &desc_set_layout) != VK_SUCCESS) {
            LOG(Error, "Failed to create descriptor set layout");
        }

        // Get descriptor pool sizes for each descriptor type
        const std::array<VkDescriptorPoolSize, 4> desc_pool_sizes  = {{
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, n_descriptors },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, n_descriptors },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, n_descriptors },
            { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, n_descriptors }
        }};

        // Create descriptor pool with UPDATE_AFTER_BIND flag set
        // todo: if device doesn't support ray tracing, only put the first 3 pools
        VkDescriptorPoolCreateInfo desc_pool_create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        desc_pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        desc_pool_create_info.maxSets = 4;
        desc_pool_create_info.poolSizeCount = desc_pool_sizes.size();
        desc_pool_create_info.pPoolSizes = desc_pool_sizes.data();

        if (vkCreateDescriptorPool(device.device, &desc_pool_create_info, nullptr, &desc_pool) != VK_SUCCESS) {
            LOG(Error, "Failed to create descriptor pool");
        }

        VkDescriptorSetAllocateInfo desc_set_alloc_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        desc_set_alloc_info.descriptorPool = desc_pool;
        desc_set_alloc_info.descriptorSetCount = 1;
        desc_set_alloc_info.pSetLayouts = &desc_set_layout;

        if (vkAllocateDescriptorSets(device.device, &desc_set_alloc_info, &desc_set) != VK_SUCCESS) {
            LOG(Error, "Failed to create descriptor set");
        }
    }
    
    ResourceHandle DescriptorHeap::alloc_descriptor(ResourceType type) {
        TODO();
        return ResourceHandle::none();
    }

    void DescriptorHeap::write_buffer_descriptor(const Device& device, ResourceHandle id, VkBuffer buffer, size_t offset, size_t size) {
        // todo: maybe batch update?
        VkDescriptorBufferInfo buffer_info = {};
        buffer_info.buffer = buffer;
        buffer_info.offset = offset;
        buffer_info.range = size;

        VkWriteDescriptorSet set = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        set.dstSet = desc_set;
        set.dstBinding = 0; // storage buffer
        set.dstArrayElement = id.id;
        set.descriptorCount = 1;
        set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        set.pBufferInfo = &buffer_info;

        vkUpdateDescriptorSets(device.device, 1, &set, 0, nullptr);
    }

    void DescriptorHeap::write_texture_descriptor(const Device &device, ResourceHandle id, VkImageLayout layout, VkImageView view) {
        VkDescriptorImageInfo image_info = {};
        image_info.imageLayout = layout;
        image_info.imageView = view;
        image_info.sampler = VK_NULL_HANDLE; // we use immutable samplers instead

        VkWriteDescriptorSet set[2] = {};
        set[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        set[0].dstSet = desc_set;
        set[0].dstBinding = 1; // sampled image
        set[0].dstArrayElement = id.id;
        set[0].descriptorCount = 1;
        set[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        set[0].pImageInfo = &image_info;
        set[1].sType =  VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        set[1].dstSet = desc_set;
        set[1].dstBinding = 2; // storage image
        set[1].dstArrayElement = id.id + 1;
        set[1].descriptorCount = 1;
        set[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        set[1].pImageInfo = &image_info;

        vkUpdateDescriptorSets(device.device, 2, set, 0, nullptr);
    }
}
