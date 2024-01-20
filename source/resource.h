#pragma once
#include "common.h"
#include <d3d12.h>

namespace gfx {
    // General
    enum class ResourceType {
        none = 0,
        texture,
    };
    inline const char* _resource_type_names[] = { "None", "Texture" };

    struct ResourceID {
        uint64_t type : 4;
        uint64_t is_loaded : 1;
        uint64_t id : 59;
    };

    struct TextureResource {
        uint32_t width, height;
        uint32_t pixel_format;
        uint8_t* data;
    };

    struct Resource {
        ResourceType type;
        ComPtr<ID3D12Resource> handle;
        union {
            TextureResource texture_resource;
        };
    };
}