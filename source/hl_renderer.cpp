#include "hl_renderer.h"

#include <memory>

#include "command.h"
#include "device.h"
#include "swapchain.h"

namespace gfx {
    std::shared_ptr<Device> _device;

    bool init_renderer(const int width, const int height, const bool debug_enabled) {
        _device = std::make_unique<Device>(width, height, debug_enabled);

        return true;
    }
}
