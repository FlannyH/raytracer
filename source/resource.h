#pragma once
#include "common.h"
#include <d3d12.h>

#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

namespace gfx {
    struct SceneNode;

    // General
    enum class ResourceType {
        none = 0,
        texture,
        buffer,
        scene,
    };
    inline const char* _resource_type_names[] = { "None", "Texture", "Buffer"};

    struct ResourceHandle {
        uint32_t id : 27;
        uint32_t is_loaded : 1;
        uint32_t type : 4;
        bool operator==(const ResourceHandle& rhs) {
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

    struct SceneResource {
        SceneNode* root;
    };

    struct BufferWithOffset {
        ResourceHandle buffer;
        uint32_t offset;
    };

    enum class PixelFormat {
        none = 0,
        rgba_8,
    };

    inline size_t size_per_pixel(const PixelFormat format) {
        switch (format)
        {
        case PixelFormat::none:
            return 0;
        case PixelFormat::rgba_8:
            return 4;
        }
        return 0;
    }

    inline size_t size_per_pixel(const uint32_t format) {
        return size_per_pixel(static_cast<PixelFormat>(format));
    }

    struct Resource {
        TextureResource& expect_texture() {
            assert(type == ResourceType::texture);
            return texture_resource;
        }
        BufferResource& expect_buffer() {
            assert(type == ResourceType::buffer);
            return buffer_resource;
        }
        SceneResource& expect_scene() {
            assert(type == ResourceType::scene);
            return scene_resource;
        }

        ResourceType type = ResourceType::none;
        ComPtr<ID3D12Resource> handle;
        union {
            // Please use expect_x() wherever you can instead of accessing this directly
            TextureResource texture_resource;
            BufferResource buffer_resource;
            SceneResource scene_resource;
        };
    };

    struct ResourceHandlePair {
        ResourceHandle handle;
        std::shared_ptr<Resource> resource;
    };

    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec4 tangent;
        glm::vec4 color;
        glm::vec2 texcoord0;
    };

    struct Triangle {
        Vertex verts[3];
    };

    inline DXGI_FORMAT pixel_format_to_dx12(PixelFormat format) {
        switch (format) {
        case PixelFormat::rgba_8:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
            break;
        }
    }
}
