#pragma once
#include <d3d12.h>
#include "common.h"

namespace gfx {
    struct DescriptorHeap {
        [[nodiscard]] DescriptorHeap new_bindless() const;
        [[nodiscard]] DescriptorHeap new_rtv() const;
    private:
        ComPtr<ID3D12DescriptorHeap> m_heap;
    };
}