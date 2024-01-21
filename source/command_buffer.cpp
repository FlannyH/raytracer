#include "command_buffer.h"

#include "device.h"

namespace gfx {
    CommandBuffer::CommandBuffer(const Device& device, ID3D12PipelineState* pipeline_state, CommandBufferType type, uint64_t fence_value_when_done) {
        m_type = type;
        switch (type) {
        case CommandBufferType::graphics:
            device.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_command_allocator));
            device.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_command_allocator.Get(), pipeline_state, IID_PPV_ARGS(&m_command_list));
            break;
        case CommandBufferType::compute:
            device.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_command_allocator));
            device.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_command_allocator.Get(), pipeline_state, IID_PPV_ARGS(&m_command_list));
            break;
        }
        m_fence_value_when_finished = fence_value_when_done;
    }
}
