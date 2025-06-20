#pragma once

#include "../common.h"
#include <vulkan/vulkan.h>

namespace gfx::vk {
    struct CommandQueue;
    struct Device;

    struct Fence {
        explicit Fence(Device* device);
        ~Fence();
        void cpu_wait(size_t value);
        void cpu_signal(size_t value) const;
        void gpu_wait(std::shared_ptr<CommandQueue> queue, size_t value) const;
        void gpu_signal(std::shared_ptr<CommandQueue> queue, size_t value) const;
        bool reached_value(size_t value);

        VkSemaphore timeline_semaphore;
        
    private:
        Device* device;
    };
}
