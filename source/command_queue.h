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
        explicit CommandQueue(ID3D12Device* device, CommandBufferType type, const std::wstring& name = L"Unnamed command queue");
        std::shared_ptr<CommandBuffer> create_command_buffer(const Pipeline* pipeline, uint64_t frame_index);
        int clean_up_old_command_buffers(uint64_t curr_finished_index);
        void execute();

    public:
        ComPtr<ID3D12CommandQueue> command_queue = nullptr;

    private:
        CommandBufferType m_type = CommandBufferType::none;
        std::vector<std::shared_ptr<CommandBuffer>> m_command_buffer_pool;
        std::vector<std::shared_ptr<CommandBuffer>> m_command_lists_to_execute;
        std::deque<size_t> m_command_buffers_to_reuse;
        std::deque<size_t> m_in_flight_command_buffers;
    };
}
