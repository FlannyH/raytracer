#include "command_queue.h"

#include "device.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include "common.h"
#include "pipeline.h"
#include "command_buffer.h"

namespace gfx {
    CommandQueue::CommandQueue(const Device& device) {
        constexpr D3D12_COMMAND_QUEUE_DESC desc = {
            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        };

        validate(device.device->CreateCommandQueue(&desc, IID_PPV_ARGS(&command_queue)));
        validate(device.device->CreateCommandAllocator(desc.Type, IID_PPV_ARGS(&m_command_allocator)));
    }

    std::shared_ptr<CommandBuffer> CommandQueue::create_command_buffer(const Device& device, std::shared_ptr<Pipeline> pipeline, CommandBufferType type) {
        return std::make_shared<CommandBuffer>(device, m_command_allocator.Get(), pipeline->pipeline_state.Get(), type);
    }
}

namespace gfx {
}