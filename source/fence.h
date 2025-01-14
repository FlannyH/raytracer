#pragma once
#include <d3d12.h>

#include "common.h"

namespace gfx {
    struct CommandQueue;
    struct Device;

    struct Fence {
        explicit Fence(const Device& device);
        ~Fence();
        void cpu_wait(size_t value);
        void cpu_signal(size_t value) const;
        void gpu_wait(std::shared_ptr<CommandQueue> queue, size_t value) const;
        void gpu_signal(std::shared_ptr<CommandQueue> queue, size_t value) const;
        bool reached_value(size_t value);

        ComPtr<ID3D12Fence> fence;
        HANDLE event_handle;
    };
}
