#include "buffer.h"
#include "hl_renderer.h"
#include "render_pass.h"

int main(int n_args, char** args) {
    const std::unique_ptr<gfx::Device> device = gfx::init_renderer(1280, 720, false);
    device->init_context();
    const auto render_pass = device->create_render_pass();
    const auto pipeline = device->create_raster_pipeline(*render_pass, "assets/shaders/test.vs.hlsl", "assets/shaders/test.ps.hlsl");

    printf("Renderer initialized!\n");

    auto texture1 = device->load_bindless_texture("assets/textures/test.png");
    std::vector<uint32_t> test_gradient;
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            test_gradient.push_back((x * 4) + ((y * 4) << 8));
        }
    }
    auto texture2 = device->load_bindless_texture("test-gradient", 64, 64, test_gradient.data(), gfx::PixelFormat::rgba_8);

    gfx::Triangle triangle {
        .verts = {
            gfx::Vertex {
                .position = {0.0, 0.5, 0.0},
                .color = {1.0, 0.0, 0.0, 1.0},
                .texcoord0 = {0.5, 1.0}
            },
            gfx::Vertex {
                .position = {-0.5, -0.5, 0.0},
                .color = {0.0, 1.0, 0.0, 1.0},
                .texcoord0 = {0.0, 0.0}
            },
            gfx::Vertex {
                .position = {0.5, -0.5, 0.0},
                .color = {0.0, 0.0, 1.0, 1.0},
                .texcoord0 = {1.0, 0.0}
            },
        }
    };
    auto triangle_vb = device->load_mesh("triangle" ,1, &triangle);
    auto monke = device->create_scene_graph_from_gltf("assets/models/hierarchy2.gltf");

    uint32_t bindings[] = { triangle_vb.id, texture1.id };
    auto bindings_buffer = device->create_buffer("bindings", sizeof(bindings), bindings);
    printf("triangle_vb = %i\n", triangle_vb.id);
    printf("bindings_buffer = %i\n", bindings_buffer.id);

    while (device->should_stay_open()) {
        device->begin_frame();
        device->test(pipeline, render_pass, bindings_buffer);
        device->end_frame();
    }
}