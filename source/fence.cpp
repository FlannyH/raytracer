#include "fence.h"
#include "device.h"
#include "command_queue.h"

namespace gfx {
    Fence::Fence(const Device& device) {
        device.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    }

    void Fence::cpu_wait(const size_t value) {
        while (m_fence->GetCompletedValue() < value);
    }

    void Fence::cpu_signal(const size_t value) const {
        m_fence->Signal(value);
    }

    void Fence::gpu_wait(const CommandQueue& queue, const size_t value) const {
        queue.command_queue->Wait(m_fence.Get(), value);
    }

    void Fence::gpu_signal(const CommandQueue& queue, const size_t value) const {
        queue.command_queue->Signal(m_fence.Get(), value);
    }
}
