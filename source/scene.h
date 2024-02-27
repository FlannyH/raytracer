#pragma once
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/gtx/quaternion.hpp>
#include <memory>
#include <vector>
#include "resource.h"

namespace gfx {
    struct Transform {
        glm::vec3 position;
        glm::quat rotation;
        glm::vec3 scale;

        Transform identity();
        glm::mat4 as_matrix();
    };

    enum class SceneNodeType : uint8_t {
        Empty,
        Mesh,
    };

    struct SceneNode {
        Transform local_transform;
        std::shared_ptr<SceneNode> parent;
        std::vector<std::shared_ptr<SceneNode>> children;

        SceneNodeType type;
        union {
            struct {
                ResourceID position_buffer;
                ResourceID attribute_buffer;
                D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC geometry;
            } mesh;
        };
    };
}