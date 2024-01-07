#include "hl_renderer.h"
#include "device.h"

namespace gfx {
    std::unique_ptr<Device> init_renderer(const int width, const int height, const bool debug_enabled) {
        return std::make_unique<Device>(width, height, debug_enabled);
    }
}
