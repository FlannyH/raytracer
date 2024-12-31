#include "buffer.h"
#include "input.h"
#include "scene.h"
#include "renderer.h"
#include <glm/gtx/transform.hpp>
#include <chrono>

int main(int n_args, char** args) {
    const auto renderer = std::make_unique<gfx::Renderer>(1280, 720, true);
    auto scene = renderer->load_scene_gltf("assets/models/DamagedHelmet.gltf");
    auto cubemap = renderer->load_environment_map("assets/textures/resting_place_2_4k.hdr", 2048, 256, 1.0f);
    auto lights = renderer->load_scene_gltf("assets/models/lights_test.glb");
    renderer->set_skybox(cubemap);

    gfx::Transform camera;
    glm::vec3 camera_euler_angles(0.0f);
    float move_speed = 1.0f;
    float mouse_sensitivity = 0.004f;

    auto prev_time = std::chrono::steady_clock::now();

    while (renderer->should_stay_open()) {
        auto curr_time = std::chrono::steady_clock::now();
        std::chrono::duration<float> duration = curr_time - prev_time;
        float delta_time = duration.count();
        prev_time = curr_time;

        input::update();

        if (input::key_held(input::Key::w)) camera.position += camera.forward_vector() * move_speed * delta_time;
        if (input::key_held(input::Key::s)) camera.position -= camera.forward_vector() * move_speed * delta_time;
        if (input::key_held(input::Key::d)) camera.position += camera.right_vector() * move_speed * delta_time;
        if (input::key_held(input::Key::a)) camera.position -= camera.right_vector() * move_speed * delta_time;
        if (input::key_held(input::Key::space)) camera.position.y += move_speed * delta_time;
        if (input::key_held(input::Key::left_shift)) camera.position.y -= move_speed * delta_time;
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
        // renderer->draw_scene(lights);
        
        renderer->end_frame();
    }
}
