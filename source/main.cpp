#include "hl_renderer.h"

int main(int n_args, char** args) {
    const std::unique_ptr<gfx::Device> device = gfx::init_renderer(1280, 720, true);
    device->init_context();
    const auto render_pass = device->create_render_pass();
    const auto pipeline = device->create_raster_pipeline(*render_pass);

    printf("Renderer initialized!\n");

    auto texture1 = device->load_bindless_texture("assets/textures/test.png");
    auto texture2 = device->load_bindless_texture("assets/textures/test.png");
    auto texture3 = device->load_bindless_texture("assets/textures/test.png");
    device->unload_bindless_resource(texture2);
    auto texture4 = device->load_bindless_texture("assets/textures/test.png");
    device->unload_bindless_resource(texture1);
    device->unload_bindless_resource(texture3);
    device->unload_bindless_resource(texture4);
    auto texture5 = device->load_bindless_texture("assets/textures/test.png");
    auto texture6 = device->load_bindless_texture("assets/textures/test.png");
    auto texture7 = device->load_bindless_texture("assets/textures/test.png");


    while (1) {
        device->begin_frame();
        device->end_frame();
    }
}