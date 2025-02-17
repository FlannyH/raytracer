
#pragma once
#include "../resource.h"

namespace gfx {
    constexpr inline VkFormat pixel_format_to_vk(const PixelFormat format) {
        switch (format) {
        case PixelFormat::r8_unorm:         return VK_FORMAT_R8_UNORM;            break;
        case PixelFormat::rg8_unorm:        return VK_FORMAT_R8G8_UNORM;          break;
        case PixelFormat::rgba8_unorm:      return VK_FORMAT_R8G8B8A8_UNORM;      break;
        case PixelFormat::rg11_b10_float:   return VK_FORMAT_UNDEFINED;           break;
        case PixelFormat::rg16_float:       return VK_FORMAT_R16G16_SFLOAT;       break;
        case PixelFormat::rgba16_float:     return VK_FORMAT_R16G16B16A16_SFLOAT; break;
        case PixelFormat::rg32_float:       return VK_FORMAT_R32G32_SFLOAT;       break;
        case PixelFormat::rgb32_float:      return VK_FORMAT_R32G32B32_SFLOAT;    break;
        case PixelFormat::rgba32_float:     return VK_FORMAT_R32G32B32A32_SFLOAT; break;
        case PixelFormat::depth32_float:    return VK_FORMAT_D32_SFLOAT;          break;
        }
        return VK_FORMAT_UNDEFINED;
    }
    constexpr inline VkImageLayout resource_usage_to_vk_image_layout(ResourceUsage usage) {
        switch (usage) {
            case ResourceUsage::none: return VK_IMAGE_LAYOUT_GENERAL;
            case ResourceUsage::read: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case ResourceUsage::compute_write: return VK_IMAGE_LAYOUT_GENERAL;
            case ResourceUsage::render_target: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            case ResourceUsage::depth_target: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            case ResourceUsage::pixel_shader_read: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case ResourceUsage::non_pixel_shader_read: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case ResourceUsage::acceleration_structure: return VK_IMAGE_LAYOUT_GENERAL; // General for acceleration structures
        }
        return VK_IMAGE_LAYOUT_GENERAL; // Default fallback
    }

    constexpr inline VkBufferUsageFlags resource_usage_to_vk_buffer_usage(ResourceUsage usage) {
        switch (usage) {
            case ResourceUsage::none:
            case ResourceUsage::read:
            case ResourceUsage::pixel_shader_read:
            case ResourceUsage::non_pixel_shader_read: 
            case ResourceUsage::cpu_read_write:
            case ResourceUsage::cpu_writable: return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; // Read-only in shaders
            case ResourceUsage::compute_write: return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            case ResourceUsage::render_target: return 0; // Not applicable for buffers
            case ResourceUsage::depth_target: return 0; // Not applicable for buffers
            case ResourceUsage::acceleration_structure: return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR; // For acceleration structures
            case ResourceUsage::copy_source: return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        }
        return 0; // Default fallback
    }
}
