#pragma once
#include "common.h"
#include <d3d12.h>

#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/matrix.hpp>

namespace gfx {
    struct SceneNode;

    enum class PixelFormat {
        none = 0,
        r8_unorm,
        rg8_unorm,
        rgba8_unorm,
        rg11_b10_float,
        rgba16_float,
        depth32_foat,
    };

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
        static ResourceHandle none() {
            return ResourceHandle{
                .id = 0,
                .is_loaded = 0,
                .type = (uint32_t)ResourceType::none,
            };
        }
        uint32_t as_u32() const {
            return id | is_loaded << 27 | type << 28;
        }
    };

    struct TextureResource {
        void* data;
        uint32_t width, height;
        PixelFormat pixel_format;
        // Extra optional handles for render targets
        glm::vec4 clear_color;
        ResourceHandle rtv_handle;
        ResourceHandle dsv_handle;
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

    inline size_t size_per_pixel(const PixelFormat format) {
        switch (format)
        {
        case PixelFormat::none:
            return 0;
        case PixelFormat::rgba8_unorm:
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
        D3D12_RESOURCE_STATES current_state = D3D12_RESOURCE_STATE_COMMON;
        std::string name;
        union {
            // Please use expect_x() wherever you can instead of accessing this directly
            TextureResource texture_resource;
            BufferResource buffer_resource;
            SceneResource scene_resource;
        };
    }; 

    struct ResourceHandlePair {
        ResourceHandle handle;
        std::shared_ptr<Resource> resource = nullptr;
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
        case PixelFormat::r8_unorm:         return DXGI_FORMAT_R8_UNORM;            break;
        case PixelFormat::rg8_unorm:        return DXGI_FORMAT_R8G8_UNORM;          break;
        case PixelFormat::rgba8_unorm:      return DXGI_FORMAT_R8G8B8A8_UNORM;      break;
        case PixelFormat::rg11_b10_float:   return DXGI_FORMAT_R11G11B10_FLOAT;     break;
        case PixelFormat::rgba16_float:     return DXGI_FORMAT_R16G16B16A16_FLOAT;  break;
        case PixelFormat::depth32_foat:     return DXGI_FORMAT_D32_FLOAT;           break;
        }
    }

    struct PacketDrawMesh {
        glm::mat4 model_transform;
        ResourceHandle vertex_buffer;
        ResourceHandle texture;
    };

    struct PacketCamera {
        glm::mat4 view_matrix;
        glm::mat4 projection_matrix;
    };
}
