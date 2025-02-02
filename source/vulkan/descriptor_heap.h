#pragma once
#include <vulkan/vulkan.h>
#include <queue>

#include "../common.h"
#include "../resource.h"
#include "device.h"

namespace gfx {
    struct DescriptorHeap {
        DescriptorHeap(const DeviceVulkan& device, uint32_t n_descriptors);
        ResourceHandle alloc_descriptor(ResourceType type);
        void free_descriptor(ResourceHandle id);
        /*
            DescriptorHeap is used to allocate resource handles, even for resources that aren't GPU 
            resources (like scene descriptions). For GPU resources, it needs to write a descriptor 
            to the descriptor set, similar to Dx12

            This means this struct needs to support these cases:
            - allocate a handle (and do nothing with the desc set)
            - allocate a handle and write a read only texture descriptor to the desc set
            - allocate a handle and write a read write texture descriptor to the desc set
            - allocate a handle and write a gpu read only buffer descriptor to the desc set
            - allocate a handle and write a gpu read and write buffer descriptor to the desc set
            - allocate a handle and write a cpu-writable buffer descriptor to the desc set
            - allocate a handle and write a cpu-writable and readable buffer descriptor to the desc set
            - allocate a handle and write an acceleration structure descriptor to the desc set
            - free descriptors, and recycle free descriptors

            Descriptor set writes could be done through functions like 
            `alloc_and_write_texture_desc(params)`, or `alloc_and_write_buffer_desc(params)`, etc.
            which allocates a handle, then based on the params creates VkWriteDescriptorSet structs,
            which then get passed to vkUpdateDescriptorSets

            Then call vkCmdBindDescriptorSets to pass it to a command buffer when rendering, which
            needs a pipeline layout, which I'll get to later with `Pipeline` struct 
        */
    };
}
