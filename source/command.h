#pragma once
#include <cstdio>
#include <d3d12.h>

#include "common.h"

namespace gfx {
    struct Device;

    struct CommandQueue {
        explicit CommandQueue(const Device& device);

    public:
        ComPtr<ID3D12CommandQueue> command_queue = nullptr;
    };
}
