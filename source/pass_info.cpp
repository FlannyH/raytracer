#include "pass_info.h"
#include "device.h"

namespace gfx {
    RasterPassInfo::RasterPassInfo(Device& device, int n_color_buffers, bool has_depth_buffer) {
        // todo: clean this up, maybe even make a FramebufferDesc struct so we can also store resolution, format, and store/load actions
        int width, height;
        device.get_window_size(width, height);

        render_to_swapchain = (n_color_buffers == 0); 
        render_depth = has_depth_buffer;

        // Create resources 
        for (int backbuffer = 0; backbuffer < backbuffer_count; ++backbuffer) {
            // For the color buffers
            color_targets[backbuffer].resize(n_color_buffers);
            for (int color_buffer = 0; color_buffer < n_color_buffers; ++color_buffer) {
                color_targets[backbuffer][color_buffer] = device.create_frame_buffer("color target", width, height, PixelFormat::rgba_8);
            }

            // For the depth buffer
            if (has_depth_buffer) {
                depth_target[backbuffer] = device.create_depth_buffer("depth target", width, height);
            }
        }
    }
    void RasterPassInfo::prepare_render(std::shared_ptr<CommandBuffer> command_buffer) {
    }
}