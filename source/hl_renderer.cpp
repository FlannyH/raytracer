#include "hl_renderer.h"

#include "command.h"
#include "device.h"
#include "swapchain.h"

namespace gfx {
    bool init_renderer(const int width, const int height, const bool debug_enabled) {
        gfx::create_window(width, height);
        gfx::init_device(debug_enabled);
        return true;
    }
}
