#pragma once
#include <cstdio>
#include <d3d12.h>
#include <deque>

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
        std::shared_ptr<CommandBuffer> create_command_buffer(const Device& device, const Pipeline* pipeline, CommandBufferType type, uint64_t frame_index);
        int clean_up_old_command_buffers(uint64_t curr_finished_index);

    public:
        ComPtr<ID3D12CommandQueue> command_queue = nullptr;

    private:
        std::vector<std::shared_ptr<CommandBuffer>> m_command_buffer_pool;
        std::deque<size_t> m_command_buffers_to_reuse;
        std::deque<size_t> m_in_flight_command_buffers;
    };
}
