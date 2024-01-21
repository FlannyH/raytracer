#include "fence.h"
#include "device.h"
#include "command_queue.h"

namespace gfx {
    Fence::Fence(const Device& device) {
        device.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    }

    void Fence::cpu_wait(const size_t value) {
        // todo: maybe we can use an event for this?
        while (fence->GetCompletedValue() < value) {};
    }

    void Fence::cpu_signal(const size_t value) const {
        fence->Signal(value);
    }

    void Fence::gpu_wait(std::shared_ptr<CommandQueue> queue, const size_t value) const {
        queue->command_queue->Wait(fence.Get(), value);
    }

    void Fence::gpu_signal(std::shared_ptr<CommandQueue> queue, const size_t value) const {
        queue->command_queue->Signal(fence.Get(), value);
    }

    bool Fence::reached_value(size_t value) {
        return fence->GetCompletedValue() >= value;
    }
}
