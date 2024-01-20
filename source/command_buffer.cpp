#include "command_buffer.h"

#include "device.h"

namespace gfx {
    CommandBuffer::CommandBuffer(const Device& device, ID3D12CommandAllocator* command_allocator, ID3D12PipelineState* pipeline_state) {

        device.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator, pipeline_state, IID_PPV_ARGS(&command_list));
    }
}
