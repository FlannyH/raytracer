#include "device.h"
#include "buffer.h"
#include <glm/gtx/transform.hpp>
#include "input.h"
#include "scene.h"

/*
* TODO:
* [ ] camera should be a separate struct with functions, out of main function
* [ ] resources should live in the DescriptorHeap, since each heap has its own indices
* [ ] make render target allocate from RTV and DSV heaps
* [ ] implement raster pass info prepare_render (transition to STATE_RENDER_TARGET)
* [ ] implement raster pass info prepare_read (transition to shader resource)
* [ ] make render target desc struct to give user more control over their render pass textures 
* [ ] implement rendering to depth texture
* [ ] maybe a raster pass info can take input resources so it can transition the states properly?
*/

int main(int n_args, char** args) {
    const auto device = std::make_unique<gfx::Device>(1280, 720, false);
    device->init_context();
    const auto pipeline = device->create_raster_pipeline("assets/shaders/test.vs.hlsl", "assets/shaders/test.ps.hlsl");
    const auto test_pass = device->create_pass_resources(1, true);

    printf("Renderer initialized!\n");

    auto scene = device->create_scene_graph_from_gltf("assets/models/hierarchy2.gltf");
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
        device->begin_raster_pass(pipeline, test_pass);
        device->set_camera(camera);
        device->draw_scene(scene.handle);
        device->end_raster_pass();
        device->end_frame();
    }
}
