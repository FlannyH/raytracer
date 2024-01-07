#pragma once
#include <memory>

#include "device.h"

namespace gfx {
    std::unique_ptr<Device> init_renderer(int width, int height, bool debug_enabled);
}
