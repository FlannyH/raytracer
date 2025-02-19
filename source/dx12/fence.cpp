#ifdef _WIN32
#include "fence.h"
#include "device.h"
#include "command_queue.h"

namespace gfx::dx12 {
    Fence::Fence(const Device& device) {
        device.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        event_handle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    }

    Fence::~Fence() {
        CloseHandle(event_handle);
    }

    void Fence::cpu_wait(const size_t value) {
        if (fence->GetCompletedValue() < value) {
            validate(fence->SetEventOnCompletion(value, event_handle));
            WaitForSingleObject(event_handle, INFINITE);
        };
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
#endif
