#pragma once
#include <d3d12.h>
#include <queue>

#include "common.h"

namespace gfx {
    struct DescriptorHeap {
        [[nodiscard]] static DescriptorHeap new_bindless_heap(size_t n_descriptors);
        [[nodiscard]] static DescriptorHeap new_rtv_heap(size_t n_descriptors);
        D3D12_CPU_DESCRIPTOR_HANDLE alloc_descriptor();

    private:
        ComPtr<ID3D12DescriptorHeap> m_heap;
        size_t m_alloc_index = 0;
        size_t m_descriptor_size = 0;
        D3D12_CPU_DESCRIPTOR_HANDLE m_start_cpu = {};
        std::deque<size_t> m_available_recycled_descriptor_indices;
    };
}