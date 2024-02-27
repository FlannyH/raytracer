#include "scene.h"
#include <glm/glm.hpp>

namespace gfx {
    Transform Transform::identity()
    {
        return Transform{
            .position = glm::vec3(0.0f, 0.0f, 0.0f),
            .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
            .scale = glm::vec3(1.0f, 1.0f, 1.0f),
        };
    }

    glm::mat4 Transform::as_matrix()
    {
        glm::mat4 mat_translate = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 mat_rotate = glm::toMat4(rotation);
        glm::mat4 mat_scale = glm::scale(glm::mat4(1.0f), scale);
        return mat_translate * mat_rotate * mat_scale;
    }
}