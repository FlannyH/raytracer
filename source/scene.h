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
        empty,
        root,
        mesh,
        light,
    };

    struct SceneNodeMesh {
        ResourceHandle position_buffer;
        ResourceHandle vertex_buffer;
        ResourceHandlePair blas;
    };
    struct SceneNodeLight {
        LightType type;
        glm::vec3 color;
        float intensity;
    };
    struct SceneNodeRoot {
        ResourceHandlePair tlas;
    };

    struct SceneNode {
        SceneNode(SceneNodeType node_type) {
            switch (node_type) {
                case SceneNodeType::empty:                         type = node_type; break;
                case SceneNodeType::mesh:  data = SceneNodeMesh{}; type = node_type; break;
                case SceneNodeType::light: data = SceneNodeLight{}; type = node_type; break;
                case SceneNodeType::root:  data = SceneNodeRoot{}; type = node_type; break;
            }
        }
        Transform local_transform;
        glm::mat4 cached_global_transform;
        glm::vec3 position_offset;
        glm::vec3 position_scale;
        std::shared_ptr<SceneNode> parent = nullptr;
        std::vector<std::shared_ptr<SceneNode>> children;
        std::string name;

        SceneNodeType type;
        void add_child_node(std::shared_ptr<SceneNode> new_child);
        auto& expect_mesh() { 
            assert(type == SceneNodeType::mesh);
            return std::get<SceneNodeMesh>(data); 
        }
        auto& expect_light() { 
            assert(type == SceneNodeType::light);
            return std::get<SceneNodeLight>(data); 
        }
        auto& expect_root() { 
            assert(type == SceneNodeType::root);
            return std::get<SceneNodeRoot>(data); 
        }

    private:
        std::variant<SceneNodeMesh, SceneNodeLight, SceneNodeRoot> data;
    };

    SceneNode* create_scene_graph_from_gltf(Renderer& renderer, const std::string& path);
}
