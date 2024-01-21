#include "descriptor_heap.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include "common.h"
#include "device.h"

namespace gfx {
    DescriptorHeap::DescriptorHeap(const Device& device, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, size_t n_descriptors) {
        const D3D12_DESCRIPTOR_HEAP_DESC desc = {
            .Type = type,
            .NumDescriptors = static_cast<UINT>(n_descriptors * 2),
            .Flags = flags,
        };

        validate(device.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)));

        m_descriptor_size = device.device->GetDescriptorHandleIncrementSize(type);
        m_start_cpu = heap->GetCPUDescriptorHandleForHeapStart();
    }
    
    // Returns the CPU descriptor handle as a size_t
    ResourceID DescriptorHeap::alloc_descriptor(ResourceType type) {
        // Find index - if there's a descriptor we can recycle, get the first one, otherwise just get the next new one
        size_t index;
        if (m_available_recycled_descriptor_indices.empty() == false) {
            index = m_available_recycled_descriptor_indices.front();
            m_available_recycled_descriptor_indices.pop_front();
        } else {
            index = m_alloc_index;
            m_alloc_index += 2;
        }
        assert(index % 2 == 0);
        assert(m_alloc_index % 2 == 0);

        printf("allocated index %i for a %s descriptor\n", index, _resource_type_names[static_cast<size_t>(type)]);

        return ResourceID {
            .type = static_cast<uint64_t>(type),
            .id = index
        };
    }

    void DescriptorHeap::free_descriptor(ResourceID id) {
        printf("freed index %i for a %s descriptor\n", id.id, _resource_type_names[id.type]);
        m_available_recycled_descriptor_indices.push_back(id.id);
        id.is_loaded = 0;
        id.id = ~0;
        id.type = static_cast<uint64_t>(ResourceType::none);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::fetch_cpu_handle(const ResourceID& id) {
        D3D12_CPU_DESCRIPTOR_HANDLE new_handle = m_start_cpu;
        new_handle.ptr += id.id * m_descriptor_size;
        return new_handle;
    }

}
