#pragma once
#include <d3d12.h>

#include "common.h"

namespace gfx {
    struct Device;

    struct Buffer {
        explicit Buffer(const Device& device, size_t size_bytes);
        explicit Buffer(const Device& device, size_t size_bytes, void* data);
        void copy_to_buffer(void* data, size_t size_bytes) const;

    public:
        ComPtr<ID3D12Resource> resource = nullptr;
        size_t size = 0;
    };
}
