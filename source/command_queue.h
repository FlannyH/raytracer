#pragma once
#include <cstdio>
#include <d3d12.h>

#include "common.h"

namespace gfx {
    struct Device;
    struct Pipeline;
    struct CommandBuffer;

    struct CommandQueue {
        explicit CommandQueue(const Device& device);

    public:
        ComPtr<ID3D12CommandQueue> command_queue = nullptr;

    private:
        ComPtr<ID3D12CommandAllocator> m_command_allocator = nullptr;
    };
}
