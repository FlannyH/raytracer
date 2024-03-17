#include "scene.h"
#include <glm/glm.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define JSON_NOEXCEPTION
#include <tinygltf/tiny_gltf.h>

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

    void SceneNode::add_child_node(std::shared_ptr<SceneNode> new_child)
    {
        children.push_back(new_child);
    }

    void traverse_nodes(std::vector<int>& node_indices, tinygltf::Model& model, glm::mat4 local_transform, std::shared_ptr<SceneNode> parent, int depth = 0) {
        // Get all child nodes
        for (auto& node_index : node_indices) {
            auto& node = model.nodes[node_index];
            for (int i = 0; i < depth; ++i) printf("\t");
            printf("node: %s\n", node.name.c_str());

            auto scene_node = std::make_shared<SceneNode>();

            // Convert matrix in gltf model to glm::mat4. If the matrix doesn't exist, just set it to identity matrix
            glm::mat4 local_matrix(1.0f);
            int i = 0;
            for (const auto& value : node.matrix) { local_matrix[i / 4][i % 4] = static_cast<float>(value); i++; }
            local_matrix = local_transform * local_matrix;

            // If it has a mesh, process it
            if (node.mesh != -1)
            {
                auto& mesh = model.meshes[node.mesh];
                auto& primitives = mesh.primitives;
                for (auto& primitive : primitives)
                {
                    // todo
                    for (int i = -1; i < depth; ++i) printf("\t");
                    printf("mesh: %s\n", mesh.name.c_str());
                    auto mesh_node = std::make_shared<SceneNode>();
                    mesh_node->type = SceneNodeType::Mesh;
                    parent->add_child_node(mesh_node);
                }
            }

            // If it has a light, process it
            if (node.light != -1) {
                auto& light = model.lights[node.light];
                for (int i = -1; i < depth; ++i) printf("\t");
                printf("%s light: %s\n", light.type.c_str(), light.name.c_str());
                auto light_node = std::make_shared<SceneNode>();
                light_node->type = SceneNodeType::Light;
                parent->add_child_node(light_node);
            }

            // If it has children, process those
            if (!node.children.empty())
            {
                traverse_nodes(node.children, model, local_matrix, scene_node, depth + 1);
            }
            parent->add_child_node(scene_node);
        }
    }

    std::shared_ptr<SceneNode> create_scene_graph_from_gltf(const std::string& path)
    {
        tinygltf::TinyGLTF loader;
        tinygltf::Model model;
        std::string error;
        std::string warning;

        if (path.ends_with(".gltf")) {
            loader.LoadASCIIFromFile(&model, &error, &warning, path);
        }
        else if (path.ends_with(".glb")) {
            loader.LoadBinaryFromFile(&model, &error, &warning, path);
        }

        // Find default scene and create a scene graph from it
        auto& scene = model.scenes[model.defaultScene];
        printf("Loading scene \"%s\"\n", scene.name.c_str());

        auto scene_node = std::make_shared<SceneNode>();
        traverse_nodes(scene.nodes, model, glm::mat4(1.0f), scene_node);
        return scene_node;
    }
}