#pragma once
#include <d3d12.h>
#include <queue>

#include "common.h"
#include "resource.h"

namespace gfx {
    struct Device;

    struct DescriptorHeap {
        DescriptorHeap(const Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, size_t n_descriptors);
        ResourceID alloc_descriptor(ResourceType type);
        void free_descriptor(ResourceID id);
        D3D12_CPU_DESCRIPTOR_HANDLE fetch_cpu_handle(const ResourceID& id);

    public:
        ComPtr<ID3D12DescriptorHeap> heap = nullptr;

    private:
        uint64_t m_alloc_index = 0;
        size_t m_descriptor_size = 0;
        D3D12_CPU_DESCRIPTOR_HANDLE m_start_cpu = {};
        std::deque<uint64_t> m_available_recycled_descriptor_indices;
    };
}
