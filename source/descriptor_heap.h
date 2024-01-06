#pragma once
#include <d3d12.h>
#include "common.h"

namespace gfx {
    struct DescriptorHeap {
        DescriptorHeap new_bindless();
        DescriptorHeap new_rtv();
    private:
        ComPtr<ID3D12DescriptorHeap> m_heap;
    };
}