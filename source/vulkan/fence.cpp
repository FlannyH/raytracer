#ifdef _WIN32
#include "fence.h"
#include "device.h"
#include "command_queue.h"

namespace gfx::vk {
    Fence::Fence(Device* device) {
        VkSemaphoreTypeCreateInfo semaphore_type_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .pNext = NULL,
            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
            .initialValue = 0
        };

        VkSemaphoreCreateInfo semaphore_create_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = &semaphore_type_info,
            .flags = 0
        };

        vkCreateSemaphore(device->device, &semaphore_create_info, NULL, &this->timeline_semaphore);
        this->device = device;
    }

    Fence::~Fence() {
        if (!this->device) {
            LOG(Error, "Destroying fence without a device?");
            return;
        }
        vkDestroySemaphore(this->device->device, this->timeline_semaphore, nullptr);
    }

    void Fence::cpu_wait(const size_t value) {
        const uint64_t value64 = (uint64_t)value;
        const VkSemaphoreWaitInfo wait_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .pNext = nullptr,
            .flags = 0,
            .semaphoreCount = 1,
            .pSemaphores = &this->timeline_semaphore,
            .pValues = &value64
        };
        vkWaitSemaphores(this->device->device, &wait_info, UINT64_MAX);
    }

    void Fence::cpu_signal(const size_t value) const {
        const VkSemaphoreSignalInfo signal_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
            .pNext = nullptr,
            .semaphore = this->timeline_semaphore,
            .value = (uint64_t)value,
        };
        vkSignalSemaphore(this->device->device, &signal_info);
    }

    // Wait for the fence to reach this value before executing the commands in the queue
    void Fence::gpu_wait(std::shared_ptr<CommandQueue> queue, const size_t value) const {
        queue->add_fence_wait_value(*this, value);
    }

    // Signal the fence value after finishing executing the commands in the queue
    void Fence::gpu_signal(std::shared_ptr<CommandQueue> queue, const size_t value) const {
        queue->add_fence_signal_value(*this, value);
    }

    bool Fence::reached_value(size_t value) {
        uint64_t curr = 0;
        vkGetSemaphoreCounterValue(this->device->device, this->timeline_semaphore, &curr);
        return curr >= value;
    }
}
#endif
