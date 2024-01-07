#include "descriptor_heap.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include "common.h"
#include "device.h"

namespace gfx {
    DescriptorHeap::DescriptorHeap(const Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t n_descriptors) {
        const D3D12_DESCRIPTOR_HEAP_DESC desc = {
            .Type = type,
            .NumDescriptors = static_cast<UINT>(n_descriptors),
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        };

        validate(device.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap)));

        m_descriptor_size = device.device->GetDescriptorHandleIncrementSize(type);
        m_start_cpu = m_heap->GetCPUDescriptorHandleForHeapStart();
    }
    
    // Returns the CPU descriptor handle as a size_t
    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::alloc_descriptor() {
        // Find index - if there's a descriptor we can recycle, get the first one, otherwise just get the next new one
        size_t index = m_alloc_index;
        if (m_available_recycled_descriptor_indices.empty() == false) {
            index = m_available_recycled_descriptor_indices.front();
            m_available_recycled_descriptor_indices.pop_front();
        }

        // Allocate
        D3D12_CPU_DESCRIPTOR_HANDLE new_handle = m_start_cpu;
        new_handle.ptr += m_alloc_index * m_descriptor_size;

        return new_handle;
    }

}
