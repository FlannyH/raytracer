#pragma once
#include <cassert>
#include <d3d12.h>

#include "common.h"

#include "command_queue.h"

namespace gfx {
    enum class CommandBufferType;
    struct CommandQueue;
    struct Device;

    struct CommandBuffer {
        CommandBuffer(const Device& device, ID3D12CommandAllocator* command_allocator, ID3D12PipelineState* pipeline_state, CommandBufferType type);

        ComPtr<ID3D12GraphicsCommandList> expect_graphics_command_list() {
            assert(m_type == CommandBufferType::graphics);
            return m_gfx_command_list;
        }
        ComPtr<ID3D12CommandList> expect_compute_command_list() {
            assert(m_type == CommandBufferType::compute);
            return m_compute_command_list;
        }


    private:
        CommandBufferType m_type;
        ComPtr<ID3D12GraphicsCommandList> m_gfx_command_list;
        ComPtr<ID3D12CommandList> m_compute_command_list;
    };
}