
#pragma once
#include "../resource.h"

namespace gfx {
    constexpr inline DXGI_FORMAT pixel_format_to_dx12(const PixelFormat format) {
        switch (format) {
        case PixelFormat::r8_unorm:         return DXGI_FORMAT_R8_UNORM;           break;
        case PixelFormat::rg8_unorm:        return DXGI_FORMAT_R8G8_UNORM;         break;
        case PixelFormat::rgba8_unorm:      return DXGI_FORMAT_R8G8B8A8_UNORM;     break;
        case PixelFormat::rg11_b10_float:   return DXGI_FORMAT_R11G11B10_FLOAT;    break;
        case PixelFormat::rg16_float:       return DXGI_FORMAT_R16G16_FLOAT;       break;
        case PixelFormat::rgba16_float:     return DXGI_FORMAT_R16G16B16A16_FLOAT; break;
        case PixelFormat::rg32_float:       return DXGI_FORMAT_R32G32_FLOAT;       break;
        case PixelFormat::rgb32_float:      return DXGI_FORMAT_R32G32B32_FLOAT;    break;
        case PixelFormat::rgba32_float:     return DXGI_FORMAT_R32G32B32A32_FLOAT; break;
        case PixelFormat::depth32_float:    return DXGI_FORMAT_D32_FLOAT;          break;
        }
        return DXGI_FORMAT_UNKNOWN;
    }

    constexpr inline D3D12_RESOURCE_DIMENSION texture_type_to_dx12_resource_dimension(const TextureType type) {
        switch (type) {
        case TextureType::tex_2d:   return D3D12_RESOURCE_DIMENSION_TEXTURE2D; break;
        case TextureType::tex_3d:   return D3D12_RESOURCE_DIMENSION_TEXTURE3D; break;
        case TextureType::tex_cube: return D3D12_RESOURCE_DIMENSION_TEXTURE2D; break;
        }
        return D3D12_RESOURCE_DIMENSION_UNKNOWN;
    }

    constexpr inline D3D12_SRV_DIMENSION texture_type_to_dx12_srv_dimension(const TextureType type) {
        switch (type) {
        case TextureType::tex_2d:   return D3D12_SRV_DIMENSION_TEXTURE2D;   break;
        case TextureType::tex_3d:   return D3D12_SRV_DIMENSION_TEXTURE3D;   break;
        case TextureType::tex_cube: return D3D12_SRV_DIMENSION_TEXTURECUBE; break;
        }
        return D3D12_SRV_DIMENSION_UNKNOWN;
    }

    constexpr inline D3D12_UAV_DIMENSION texture_type_to_dx12_uav_dimension(const TextureType type) {
        switch (type) {
        case TextureType::tex_2d:   return D3D12_UAV_DIMENSION_TEXTURE2D;      break;
        case TextureType::tex_3d:   return D3D12_UAV_DIMENSION_TEXTURE3D;      break;
        case TextureType::tex_cube: return D3D12_UAV_DIMENSION_TEXTURE2DARRAY; break;
        }
        return D3D12_UAV_DIMENSION_UNKNOWN;
    }
}
