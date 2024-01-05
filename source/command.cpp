#include "device.h"
#include <glfw/glfw3.h>
#include <glfw/glfw3native.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <exception>
#include <wrl.h>

#include "common.h"

using Microsoft::WRL::ComPtr;

namespace gfx {
    extern ComPtr<ID3D12Device> _device;
    ComPtr<ID3D12CommandQueue> _command_queue = nullptr;
    ComPtr<ID3D12CommandAllocator> _command_allocator = nullptr;
}

namespace gfx {
    void create_command_queue() {
        if (_command_queue == nullptr) {
            constexpr D3D12_COMMAND_QUEUE_DESC desc = {
                .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
                .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
            };
            validate(_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&_command_queue)));
            validate(_device->CreateCommandAllocator(desc.Type,
                IID_PPV_ARGS(&_command_allocator)));
        }
    }
}