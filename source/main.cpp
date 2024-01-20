#include "hl_renderer.h"

int main(int n_args, char** args) {
    const std::unique_ptr<gfx::Device> device = gfx::init_renderer(1280, 720, true);
    device->init_context();
    const auto render_pass = device->create_render_pass();
    const auto pipeline = device->create_raster_pipeline(*render_pass);

    printf("Renderer initialized!\n");

    auto texture1 = device->load_bindless_texture("assets/textures/test.png");
    std::vector<uint32_t> test_gradient;
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            test_gradient.push_back(x * 4 + y * 4 << 8);
        }
    }
    auto texture2 = device->load_bindless_texture(64, 64, test_gradient.data(), gfx::PixelFormat::rgba_8);


    while (1) {
        device->begin_frame();
        device->end_frame();
    }
}