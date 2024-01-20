#include "buffer.h"
#include "device.h"

namespace gfx {
    Buffer::Buffer(const Device& device, const size_t size_bytes) {
        D3D12_HEAP_PROPERTIES upload_heap_props = {
            D3D12_HEAP_TYPE_UPLOAD, // The heap will be used to upload data to the GPU
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            D3D12_MEMORY_POOL_UNKNOWN, 1, 1
        };

        D3D12_RESOURCE_DESC upload_buffer_desc = {
            D3D12_RESOURCE_DIMENSION_BUFFER, // Can either be texture or buffer, we want a buffer
            D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
            size_bytes,
            1,
            1,
            1,
            DXGI_FORMAT_UNKNOWN, // This is only really useful for textures, so for buffer this is unknown
            {1, 0}, // Texture sampling quality settings, not important for non-textures, so set it to lowest
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR, // First left to right, then top to bottom
            D3D12_RESOURCE_FLAG_NONE,
        };

        device.device->CreateCommittedResource(&upload_heap_props, D3D12_HEAP_FLAG_NONE, &upload_buffer_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), &resource);

        size = size_bytes;
    }

    Buffer::Buffer(const Device& device, const size_t size_bytes, void* data) : Buffer(device, size_bytes) {
        copy_to_buffer(data, size_bytes);
    }

    void Buffer::copy_to_buffer(void* data, size_t size_bytes) const {
        D3D12_RANGE range{ 0, 0 };
        void* begin;
        resource->Map(0, &range, &begin);
        memcpy(begin, data, std::min(size_bytes, this->size));
        resource->Unmap(0, nullptr);
    }
}
