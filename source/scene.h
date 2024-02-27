#pragma once
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include<glm/gtx/quaternion.hpp>

namespace gfx {
    struct Transform {
        glm::vec3 position;
        glm::quat rotation;
        glm::vec3 scale;

        Transform identity();
        glm::mat4 as_matrix();
    };
}