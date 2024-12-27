#pragma once
#include "common.h"
#include <d3d12.h>

#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/matrix.hpp>
#include <optional>

namespace gfx {
    struct SceneNode;

    enum class PixelFormat {
        none = 0,
        r8_unorm,
        rg8_unorm,
        rgba8_unorm,
        rg11_b10_float,
        rgba16_float,
        rgba32_float,
        depth32_float,
    };

    enum class TextureType {
        tex_2d,
        tex_3d,
        tex_cube
    };

    // General
    enum class ResourceType {
        none = 0,
        texture,
        buffer,
        scene,
    };
    inline const char* _resource_type_names[] = { "None", "Texture", "Buffer"};

    enum class ResourceUsage : uint8_t {
        none = 0,
        read,
        render_target,
        depth_target,
        compute_write,
    };

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
        uint32_t as_u32_uav() const {
            return (id+1) | is_loaded << 27 | type << 28;
        }
    };

    struct TextureResource {
        void* data;
        uint32_t width, height, depth;
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

    constexpr inline size_t size_per_pixel(const PixelFormat format) {
        switch (format)
        {
        case PixelFormat::none:
            return 0;
        case PixelFormat::r8_unorm:
            return 1;
        case PixelFormat::rg8_unorm:
            return 2;
        case PixelFormat::rgba8_unorm:
            return 4;
        case PixelFormat::rg11_b10_float:
            return 4;
        case PixelFormat::rgba16_float:
            return 8;
        case PixelFormat::rgba32_float:
            return 16;
        case PixelFormat::depth32_float:
            return 4;
        }
        return 0;
    }

    constexpr inline size_t size_per_pixel(const uint32_t format) {
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
        ResourceUsage usage = ResourceUsage::none;
        ComPtr<ID3D12Resource> handle;
        D3D12_RESOURCE_STATES current_state = D3D12_RESOURCE_STATE_COMMON;
        std::string name;
     private:
        union {
            TextureResource texture_resource;
            BufferResource buffer_resource;
            SceneResource scene_resource;
        };
    }; 

    struct ResourceHandlePair {
        ResourceHandle handle = ResourceHandle::none();
        std::shared_ptr<Resource> resource = nullptr;
    };

    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec4 tangent;
        glm::vec4 color;
        glm::vec2 texcoord0;
        uint32_t material_id;
    };

    struct VertexFlags1 {
        uint8_t tangent_sign : 1; // Tangent vector's sign. 1 = positive, 0 = negative
        uint8_t _reserved : 7;
    };
    struct VertexFlags2 {
        uint8_t _reserved : 8;
    };

    struct VertexCompressed {
        glm::u16vec3 position; // 1.14 fixed point positions that need to be dequantized by the mesh's corresponding scaling vectors
        uint16_t material_id; // Index into the material array. 0xFFFF means no material -> use default material
        glm::u8vec3 normal; // Normal vector, where 0 = -1.0, 127 = 0.0, 254 = +1.0, kinda like a normal map texture
        VertexFlags1 flags1;
        glm::u8vec3 tangent; // Tangent vector, where 0 = -1.0, 127 = 0.0, 254 = +1.0, just like the normal vector
        VertexFlags2 flags2;
        glm::u16vec4 color; // Linear RGB 0-1023 for SDR, with brighter HDR colors above that. Alpha is in range 0 - 1023, and values above that should be clamped to 1023 (1.0)
        glm::vec2 texcoord0;
    };

    struct Triangle {
        Vertex verts[3];
    };

    constexpr inline DXGI_FORMAT pixel_format_to_dx12(const PixelFormat format) {
        switch (format) {
        case PixelFormat::r8_unorm:         return DXGI_FORMAT_R8_UNORM;            break;
        case PixelFormat::rg8_unorm:        return DXGI_FORMAT_R8G8_UNORM;          break;
        case PixelFormat::rgba8_unorm:      return DXGI_FORMAT_R8G8B8A8_UNORM;      break;
        case PixelFormat::rg11_b10_float:   return DXGI_FORMAT_R11G11B10_FLOAT;     break;
        case PixelFormat::rgba16_float:     return DXGI_FORMAT_R16G16B16A16_FLOAT;  break;
        case PixelFormat::rgba32_float:     return DXGI_FORMAT_R32G32B32A32_FLOAT;  break;
        case PixelFormat::depth32_float:    return DXGI_FORMAT_D32_FLOAT;          break;
        }
        return DXGI_FORMAT_UNKNOWN;
    }

    constexpr inline D3D12_RESOURCE_DIMENSION texture_type_to_dx12_resource_dimension(const TextureType type) {
        switch (type) {
        case TextureType::tex_2d:   return D3D12_RESOURCE_DIMENSION_TEXTURE2D;  break;
        case TextureType::tex_3d:   return D3D12_RESOURCE_DIMENSION_TEXTURE3D;  break;
        case TextureType::tex_cube: return D3D12_RESOURCE_DIMENSION_TEXTURE2D;  break;
        }
        return D3D12_RESOURCE_DIMENSION_UNKNOWN;
    }

    constexpr inline D3D12_SRV_DIMENSION texture_type_to_dx12_srv_dimension(const TextureType type) {
        switch (type) {
        case TextureType::tex_2d:   return D3D12_SRV_DIMENSION_TEXTURE2D;  break;
        case TextureType::tex_3d:   return D3D12_SRV_DIMENSION_TEXTURE3D;  break;
        case TextureType::tex_cube: return D3D12_SRV_DIMENSION_TEXTURECUBE;  break;
        }
        return D3D12_SRV_DIMENSION_UNKNOWN;
    }

    constexpr inline D3D12_UAV_DIMENSION texture_type_to_dx12_uav_dimension(const TextureType type) {
        switch (type) {
        case TextureType::tex_2d:   return D3D12_UAV_DIMENSION_TEXTURE2D;  break;
        case TextureType::tex_3d:   return D3D12_UAV_DIMENSION_TEXTURE3D;  break;
        case TextureType::tex_cube: return D3D12_UAV_DIMENSION_TEXTURE2DARRAY;  break;
        }
        return D3D12_UAV_DIMENSION_UNKNOWN;
    }

    struct PacketDrawMesh {
        glm::mat4 model_transform;
        glm::vec4 position_offset;
        glm::vec4 position_scale;
        ResourceHandle vertex_buffer;
    };

    struct PacketCamera {
        glm::mat4 view_matrix;
        glm::mat4 projection_matrix;
    };

    struct Material {
        glm::vec4 color_multiplier = { 1.0f, 1.0f, 1.0f, 1.0f }; // Color to multiply the color texture with.
        glm::vec3 emissive_multiplier = { 1.0f, 1.0f, 1.0f }; // Color to multiply the emissive texture with.
        ResourceHandle color_texture = ResourceHandle::none(); // If set to none, a default value of { 1, 1, 1, 1 } will be used.
        ResourceHandle normal_texture = ResourceHandle::none(); // If set to none, a default value of { 0.5, 0.5, 1.0 } will be used.
        ResourceHandle metal_roughness_texture = ResourceHandle::none(); // If set to none, a default value of { 0.0, 1.0 } will be used.
        ResourceHandle emissive_texture = ResourceHandle::none(); // If set to none, a default value of { 0, 0, 0 } will be used.
        float normal_intensity = 1.0f; // Used to interpolate between { 0.5, 0.5, 1.0 } and the sampled normal map value. Can go beyond 1.0 to make the normal map more intense
        float roughness_multiplier = 1.0f; // Will be multipled with the sampled roughness texture value
        float metallic_multiplier = 1.0f; // Will be multiplied with the sample metallic texture value
        uint64_t reserved = 0; // This makes the struct size 64 bytes, perfect for cache lines
    };

    enum class LightType {
        Directional,
        Point,
        Spot
    };

    struct LightDirectional {
        glm::vec3 color; // linear 0.0 - 1.0
        float intensity; // in lux (lm/m^2)
        glm::vec3 direction;
    };

    struct LightPoint {
        glm::vec3 color; // linear 0.0 - 1.0
        float intensity; // in candela (lm/sr)
        glm::vec3 position;
    };

    struct LightSpot {
        glm::vec3 color; // linear 0.0 - 1.0
        float intensity; // in candela (lm/sr)
        glm::vec3 position;
        glm::vec3 direction;
        float inner_cone_angle; // radians
        float outer_cone_angle; // radians
    };

    struct Cubemap {
        std::optional<ResourceHandlePair> hdri{};
        ResourceHandlePair base{};
        ResourceHandlePair ibl_specular{};
        uint32_t offset_diffuse_sh = 0;
    };
}
