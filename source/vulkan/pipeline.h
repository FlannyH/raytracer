#pragma once

#include <vulkan/vulkan.h>
#include "../common.h"

namespace gfx::vk {
    struct Device;
    struct RenderPass;

    struct Pipeline {
        explicit Pipeline(const Device& device, const std::string& name, const std::string& vertex_shader_path, const std::string& pixel_shader_path, const std::vector<VkFormat> render_target_formats, const VkFormat depth_target_format = VK_FORMAT_UNDEFINED);
        explicit Pipeline(const Device& device, const std::string& name, const std::string& compute_shader_path);
        const std::string& get_name() { return name; }

    private:
        std::string name;
        void create_global_root_signature(const Device& device);
    };
}
