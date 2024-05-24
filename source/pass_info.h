#pragma once
#include <vector>
#include "resource.h"

namespace gfx {
    struct Device;

    struct RasterPassInfo {
        // The user can specify multiple color targets for one render pass. Useful for things like deferred rendering. May be empty if `render_to_swapchain` is set to true.
        std::vector<ResourceHandlePair> color_targets[backbuffer_count];
        ResourceHandlePair depth_target[backbuffer_count];
        bool render_to_swapchain = true;
        bool render_depth = false;

        // If you want to render directly to swapchain, set n_color_buffers to 0
        RasterPassInfo(Device& device, int n_color_buffers, bool has_depth_buffer);
        void prepare_render(std::shared_ptr<CommandBuffer> command_buffer);
    };
}