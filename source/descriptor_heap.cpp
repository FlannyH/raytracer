#include "descriptor_heap.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include "common.h"

namespace gfx {
    extern ComPtr<ID3D12Device> _device;
    extern ComPtr<IDXGISwapChain3> _swapchain;
}

namespace gfx {
    DescriptorHeap DescriptorHeap::new_bindless_heap(const size_t n_descriptors) {
        DescriptorHeap heap;

        const D3D12_DESCRIPTOR_HEAP_DESC desc = {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = static_cast<UINT>(n_descriptors),
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        };

        validate(_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap.m_heap)));

        heap.m_descriptor_size = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        heap.m_start_cpu = heap.m_heap->GetCPUDescriptorHandleForHeapStart();

        return heap;
    }

    DescriptorHeap DescriptorHeap::new_rtv_heap(const size_t n_descriptors) {
        DescriptorHeap heap;

        const D3D12_DESCRIPTOR_HEAP_DESC desc = {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            .NumDescriptors = static_cast<UINT>(n_descriptors),
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        };

        validate(_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap.m_heap)));

        heap.m_descriptor_size = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        heap.m_start_cpu = heap.m_heap->GetCPUDescriptorHandleForHeapStart();

        return heap;
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
