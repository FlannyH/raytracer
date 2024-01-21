#include "command_buffer.h"

#include "device.h"

namespace gfx {
    CommandBuffer::CommandBuffer(const Device& device, ID3D12CommandAllocator* command_allocator, ID3D12PipelineState* pipeline_state, const CommandBufferType type) {
        m_type = type;
        switch (type) {
        case CommandBufferType::graphics:
            device.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator, pipeline_state, IID_PPV_ARGS(&m_gfx_command_list));
            break;
        case CommandBufferType::compute:
            device.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, command_allocator, pipeline_state, IID_PPV_ARGS(&m_compute_command_list));
            break;
        }
    }
}
