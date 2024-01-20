#include "hl_renderer.h"

int main(int n_args, char** args) {
    const std::unique_ptr<gfx::Device> device = gfx::init_renderer(1280, 720, false);
    device->init_context();
    const auto render_pass = device->create_render_pass();
    const auto pipeline = device->create_raster_pipeline(*render_pass);

    printf("Renderer initialized!\n");

    while (1) {
        device->begin_frame();
        device->end_frame();
    }
}