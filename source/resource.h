#pragma once
#include "common.h"
#include <d3d12.h>

#include <glm/vec3.hpp>

namespace gfx {
    // General
    enum class ResourceType {
        none = 0,
        texture,
        buffer,
    };
    inline const char* _resource_type_names[] = { "None", "Texture", "Buffer"};

    struct ResourceID {
        uint64_t type : 4;
        uint64_t is_loaded : 1;
        uint64_t id : 59;
        bool operator==(const ResourceID& rhs) {
            // We only really need to check if the IDs are identical, but we should sanity check the rest too
            assert(type == rhs.type);
            assert(is_loaded == rhs.is_loaded);
            return id == rhs.id;
        }
    };

    struct TextureResource {
        void* data;
        uint32_t width, height;
        uint32_t pixel_format;
    };

    struct BufferResource {
        void* data;
        uint64_t size;
    };

    enum class PixelFormat {
        none = 0,
        rgba_8
    };

    struct Resource {
        TextureResource& expect_texture() {
            assert(type == ResourceType::texture);
            return texture_resource;
        }
        BufferResource& expect_buffer() {
            assert(type == ResourceType::buffer);
            return buffer_resource;
        }

        ResourceType type = ResourceType::none;
        ComPtr<ID3D12Resource> handle;
        union {
            // Please use expect_x() wherever you can instead of this
            TextureResource texture_resource;
            BufferResource buffer_resource;
        };
    };

    struct Vertex {
        glm::vec3 position;
        glm::vec3 color;
    };

    struct Triangle {
        Vertex verts[3];
    };
}