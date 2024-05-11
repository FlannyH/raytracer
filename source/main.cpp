#include "device.h"
#include "buffer.h"

int main(int n_args, char** args) {
    const std::unique_ptr<gfx::Device> device = std::make_unique<gfx::Device>(1280, 720, false);
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

    float scale = 0.1f;

    while (device->should_stay_open()) {
        scale += 0.001f;
        device->begin_frame();
        device->begin_raster_pass(pipeline, gfx::RasterPassInfo{
            .color_target = gfx::ResourceHandle::none()
        });
        device->draw_mesh(gfx::DrawPacket{
            .model_transform = glm::mat3x4(scale),
            .vertex_buffer = triangle_vb.handle,
            .texture = texture1.handle,
        });
        device->end_raster_pass();
        device->end_frame();
    }
}