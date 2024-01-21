#pragma once
#include <cstdio>
#include <d3d12.h>

#include "common.h"

namespace gfx {
    struct Device;
    struct Pipeline;
    struct CommandBuffer;

    enum class CommandBufferType {
        none = 0,
        graphics,
        compute
    };

    struct CommandQueue {
        explicit CommandQueue(const Device& device);
        std::shared_ptr<CommandBuffer> create_command_buffer(const Device& device, std::shared_ptr<Pipeline> pipeline, CommandBufferType type);

    public:
        ComPtr<ID3D12CommandQueue> command_queue = nullptr;

    private:
        ComPtr<ID3D12CommandAllocator> m_command_allocator = nullptr;
    };
}
