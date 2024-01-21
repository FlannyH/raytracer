#pragma once
#include <cassert>
#include <d3d12.h>

#include "common.h"

#include "command_queue.h"

namespace gfx {
    struct Fence;
    enum class CommandBufferType;
    struct CommandQueue;
    struct Device;

    struct CommandBuffer {
        CommandBuffer(const Device& device, ID3D12PipelineState* pipeline_state, CommandBufferType type, uint64_t fence_value_when_done);

        ID3D12GraphicsCommandList* expect_graphics_command_list() {
            assert(m_type == CommandBufferType::graphics);
            return m_command_list.Get();
        }
        ID3D12CommandList* expect_compute_command_list() {
            assert(m_type == CommandBufferType::compute);
            return m_command_list.Get();
        }

        void reset(ID3D12PipelineState* pipeline_state, uint64_t frame_index) {
            m_command_allocator->Reset();
            m_command_list->Reset(m_command_allocator.Get(), pipeline_state);
            fence_value_when_finished = frame_index;
        }

    public:
        uint64_t fence_value_when_finished = ~0;

    private:
        CommandBufferType m_type = CommandBufferType::none;
        ComPtr<ID3D12GraphicsCommandList> m_command_list;
        ComPtr<ID3D12CommandAllocator> m_command_allocator;
    };
}