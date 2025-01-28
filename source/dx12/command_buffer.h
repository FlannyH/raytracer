#pragma once
#include <cassert>
#include <d3d12.h>

#include "../common.h"

#include "command_queue.h"

namespace gfx {
    struct Fence;
    enum class CommandBufferType;
    struct CommandQueue;

    struct CommandBuffer {
        CommandBuffer(ID3D12Device* device, ID3D12PipelineState* pipeline_state, CommandBufferType type, uint64_t fence_value_when_done);

        ID3D12GraphicsCommandList* get() {
            return m_command_list.Get();
        }

        ID3D12GraphicsCommandList4* get_rt() {
            ID3D12GraphicsCommandList4* cmd;
            HRESULT hr = m_command_list->QueryInterface(IID_PPV_ARGS(&cmd));
            if (FAILED(hr)) {
                LOG(Error, "Failed to get ID3D12GraphicsCommandList4* interface");
                return nullptr;
            }
            return cmd;
        }

        void reset(ID3D12PipelineState* pipeline_state, uint64_t frame_index) {
            m_command_allocator->Reset();
            m_command_list->Reset(m_command_allocator.Get(), pipeline_state);
            m_fence_value_when_finished = frame_index;
        }

        bool is_finished(uint64_t curr_fence_value) {
            return curr_fence_value >= m_fence_value_when_finished;
        }

    private:
        CommandBufferType m_type = CommandBufferType::none;
        ComPtr<ID3D12GraphicsCommandList> m_command_list;
        ComPtr<ID3D12CommandAllocator> m_command_allocator;
        uint64_t m_fence_value_when_finished = ~0;
    };
}
