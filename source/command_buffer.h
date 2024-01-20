#pragma once
#include <d3d12.h>

#include "common.h"

namespace gfx {
    struct CommandQueue;
    struct Device;

    struct CommandBuffer {
        CommandBuffer(const Device& device, ID3D12CommandAllocator* command_allocator, ID3D12PipelineState* pipeline_state);

    public:
        ComPtr<ID3D12CommandList> command_list;
    };
}