#include "buffer.h"
#include <glm/gtx/transform.hpp>
#include "input.h"
#include "scene.h"
#include "renderer.h"

int main(int n_args, char** args) {
    const auto renderer = std::make_unique<gfx::Renderer>(1280, 720, true);
    auto scene = renderer->load_scene_gltf("assets/models/hierarchy2.gltf");

    gfx::Transform camera;
    glm::vec3 camera_euler_angles(0.0f);
    float move_speed = 0.01f;
    float mouse_sensitivity = 0.004f;

    while (renderer->should_stay_open()) {
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

        renderer->begin_frame();
        
        renderer->set_camera(camera);
        renderer->draw_scene(scene);
        
        renderer->end_frame();
    }
}
