#pragma once
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/gtx/quaternion.hpp>
#include <memory>
#include <vector>
#include "resource.h"
#include "renderer.h"

namespace gfx {
    struct Transform {
        glm::vec3 position{ 0, 0, 0 };
        glm::quat rotation{ 1, 0, 0, 0 };
        glm::vec3 scale{ 1, 1, 1 };

        glm::mat4 as_matrix();
        glm::mat4 as_view_matrix();
        glm::vec3 forward_vector();
        glm::vec3 right_vector();
        glm::vec3 up_vector();
    };

    enum class SceneNodeType : uint8_t {
        Empty,
        Mesh,
        Light,
    };

    struct SceneNode {
        Transform local_transform;
        glm::mat4 cached_global_transform;
        glm::vec3 position_offset;
        glm::vec3 position_scale;
        std::shared_ptr<SceneNode> parent = nullptr;
        std::vector<std::shared_ptr<SceneNode>> children;
        std::string name;

        SceneNodeType type;
        union {
            struct {
                ResourceHandle position_buffer;
                ResourceHandle vertex_buffer;
                D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC geometry;
            } mesh;
            struct {
                LightType type;
                glm::vec3 color;
                float intensity;
            } light;
        };

        void add_child_node(std::shared_ptr<SceneNode> new_child);
    };

    SceneNode* create_scene_graph_from_gltf(Renderer& renderer, const std::string& path);
}
