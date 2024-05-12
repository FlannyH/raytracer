#include "device.h"
#include "buffer.h"
#include <glm/gtx/transform.hpp>
#include "input.h"

int main(int n_args, char** args) {
    const auto device = std::make_unique<gfx::Device>(1280, 720, false);
    device->init_context();
    const auto pipeline = device->create_raster_pipeline("assets/shaders/test.vs.hlsl", "assets/shaders/test.ps.hlsl");

    printf("Renderer initialized!\n");

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
    auto texture1 = device->load_texture("assets/textures/test.png");

    float scale = 1.00f;

    while (device->should_stay_open()) {
        input::update();

        if (input::key_held(input::Key::up))
            scale *= 1.005f;
        if (input::key_held(input::Key::down))
            scale /= 1.005f;

        scale += 0.05f * input::mouse_scroll().y;

        device->begin_frame();
        device->begin_raster_pass(pipeline, gfx::RasterPassInfo{
            .color_target = gfx::ResourceHandle::none()
        });
        device->draw_mesh(gfx::DrawPacket{
            .model_transform = glm::scale(glm::vec3(scale)),
            .vertex_buffer = triangle_vb.handle,
            .texture = texture1.handle,
        });
        device->end_raster_pass();
        device->end_frame();
    }
}