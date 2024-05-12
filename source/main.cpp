#include "device.h"
#include "buffer.h"
#include <glm/gtx/transform.hpp>
#include "input.h"
#include "scene.h"

int main(int n_args, char** args) {
    const auto device = std::make_unique<gfx::Device>(1280, 720, false);
    device->init_context();
    const auto pipeline = device->create_raster_pipeline("assets/shaders/test.vs.hlsl", "assets/shaders/test.ps.hlsl");

    printf("Renderer initialized!\n");

    gfx::Triangle triangle {
        .verts = {
            gfx::Vertex {
                .position = {0.0, 0.5, -1.0},
                .color = {1.0, 0.0, 0.0, 1.0},
                .texcoord0 = {0.5, 1.0}
            },
            gfx::Vertex {
                .position = {-0.5, -0.5, -1.0},
                .color = {0.0, 1.0, 0.0, 1.0},
                .texcoord0 = {0.0, 0.0}
            },
            gfx::Vertex {
                .position = {0.5, -0.5, -1.0},
                .color = {0.0, 0.0, 1.0, 1.0},
                .texcoord0 = {1.0, 0.0}
            },
        }
    };
    auto scene = device->create_scene_graph_from_gltf("assets/models/hierarchy2.gltf");
    auto triangle_vb = device->load_mesh("triangle", 1, &triangle);
    auto texture1 = device->load_texture("assets/textures/test.png");

    gfx::Transform camera;
    glm::vec3 camera_euler_angles(0.0f);
    float move_speed = 0.01f;
    float mouse_sensitivity = 0.004f;

    while (device->should_stay_open()) {
        input::update();

        if (input::key_held(input::Key::w)) camera.position += camera.forward_vector() * move_speed;
        if (input::key_held(input::Key::s)) camera.position -= camera.forward_vector() * move_speed;
        if (input::key_held(input::Key::d)) camera.position += camera.right_vector() * move_speed;
        if (input::key_held(input::Key::a)) camera.position -= camera.right_vector() * move_speed;
        if (input::key_held(input::Key::space)) camera.position.y += move_speed;
        if (input::key_held(input::Key::left_shift)) camera.position.y -= move_speed;
        if (input::mouse_button(input::MouseButton::right)) {
            camera_euler_angles.y -= input::mouse_movement().x * mouse_sensitivity;
            camera_euler_angles.x -= input::mouse_movement().y * mouse_sensitivity;
            glm::clamp(camera_euler_angles.x, glm::radians(-90.f), glm::radians(90.f));
            glm::mod(camera_euler_angles.y, glm::radians(360.f));
            camera.rotation = glm::quat(camera_euler_angles);
        }

        device->begin_frame();
        device->begin_raster_pass(pipeline, gfx::RasterPassInfo{
            .color_target = gfx::ResourceHandle::none()
        });
        device->set_camera(camera);
        //device->draw_mesh(gfx::DrawPacket{
        //    .draw_mesh = {
        //        .model_transform = glm::mat4(1.0f),
        //        .vertex_buffer = triangle_vb.handle,
        //        .texture = gfx::ResourceHandle::none()
        //    },
        //});
        device->draw_scene(scene.handle);
        device->end_raster_pass();
        device->end_frame();
    }
}