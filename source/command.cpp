#include "command.h"

#include "device.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include "common.h"

namespace gfx {
    ComPtr<ID3D12CommandAllocator> _command_allocator = nullptr;

    CommandQueue::CommandQueue(const Device& device) {
        constexpr D3D12_COMMAND_QUEUE_DESC desc = {
            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        };

        validate(device.device->CreateCommandQueue(&desc, IID_PPV_ARGS(&command_queue)));
        validate(device.device->CreateCommandAllocator(desc.Type, IID_PPV_ARGS(&_command_allocator)));

    }
}

namespace gfx {
}