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
    };
}
