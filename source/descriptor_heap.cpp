#include "descriptor_heap.h"

#include <d3d12.h>

#include "common.h"

namespace gfx {
    extern ComPtr<ID3D12Device> _device;
}

namespace gfx {
    DescriptorHeap DescriptorHeap::new_bindless() const {
        DescriptorHeap heap;

        constexpr D3D12_DESCRIPTOR_HEAP_DESC desc = {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        };

        validate(_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap.m_heap)));

        return heap;
    }

    DescriptorHeap DescriptorHeap::new_rtv() const {
        DescriptorHeap heap;

        constexpr D3D12_DESCRIPTOR_HEAP_DESC desc = {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            .NumDescriptors = backbuffer_count,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        };

        validate(_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap.m_heap)));

        return heap;
    }

}