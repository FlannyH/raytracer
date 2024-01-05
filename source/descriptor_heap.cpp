#include <d3d12.h>

#include "common.h"

namespace gfx {
    extern constexpr UINT backbuffer_count = 3;
    extern ComPtr<ID3D12Device> _device;

    ComPtr<ID3D12DescriptorHeap> _render_target_view_heap;
}

namespace gfx {
    void create_descriptor_heaps() {
        constexpr D3D12_DESCRIPTOR_HEAP_DESC render_target_view_heap_desc = {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            .NumDescriptors = backbuffer_count,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        };
        validate(_device->CreateDescriptorHeap(&render_target_view_heap_desc, IID_PPV_ARGS(&_render_target_view_heap)));
    }
}