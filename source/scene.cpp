#include "scene.h"
#include <glm/glm.hpp>
#include <limits>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define JSON_NOEXCEPTION
#include <tinygltf/tiny_gltf.h>

namespace gfx {
    glm::mat4 Transform::as_matrix() {
        glm::mat4 mat_translate = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 mat_rotate = glm::toMat4(rotation);
        glm::mat4 mat_scale = glm::scale(glm::mat4(1.0f), scale);
        return mat_translate * mat_rotate * mat_scale;
    }

    glm::mat4 Transform::as_view_matrix()
    {
        return (glm::lookAt(
            position,
            position + forward_vector(),
            up_vector()));
    }

    glm::vec3 Transform::forward_vector() {
        return rotation * glm::vec3{ 0,0,-1 };
    }

    glm::vec3 Transform::right_vector() {
        return rotation * glm::vec3{ 1,0,0 };
    }

    glm::vec3 Transform::up_vector() {
        return rotation * glm::vec3{ 0,1,0 };
    }

    void SceneNode::add_child_node(std::shared_ptr<SceneNode> new_child) {
        children.push_back(new_child);
    }

    /// Takes the input array, casts all the entries in the array to the output type, and then returns a pointer to the converted array. 
    template<typename In, typename Out>
    Out* convert_array(In* input, const size_t input_size_bytes, size_t& n_values, const bool normalized) {
        // Thank you Microsoft
        #undef min
        #undef max
        constexpr Out min_ = (Out)std::numeric_limits<In>::min();
        constexpr Out max_ = (Out)std::numeric_limits<In>::max();
        const size_t n_values_ = input_size_bytes / sizeof(In);
        Out* output = new Out[n_values_];
        for (size_t i = 0; i < n_values_; ++i) {
            if (normalized) {
                output[i] = std::clamp(((Out)input[i]) / max_, min_, max_);
            }
            else { 
                output[i] = (Out)input[i]; 
            }
        }
        n_values = n_values_;
        return output;
    }

    /// Takes the input array, casts all entries to ComponentType, converts it to ComponentType, and returns a vector of the Out type
    /// `Out` is expected to be a floating point type such as `float`, `glm::mat4`, `glm::vec3`, etc
    /// `ComponentType` is the component type of `Out`, so if `Out` is a `glm::vec3`, the `ComponentType` should be `float`
    /// `default_value` is used in case we want more components than we have, so then the last values will take the default value's component
    template<typename ComponentType, typename Out>
    std::vector<Out> convert_gltf_buffer(void* input, int component_type, size_t component_stride, Out default_value, size_t size_bytes, const bool normalized) {
        ComponentType* converted_array = nullptr;
        size_t size_component = 0;
        size_t n_values = 0;

        switch (component_type) {
        case TINYGLTF_COMPONENT_TYPE_BYTE:
            converted_array = convert_array<int8_t, ComponentType>((int8_t*)input, size_bytes, n_values, normalized);
            size_component = 1;
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            converted_array = convert_array<uint8_t, ComponentType>((uint8_t*)input, size_bytes, n_values, normalized);
            size_component = 1;
            break;
        case TINYGLTF_COMPONENT_TYPE_SHORT:
            converted_array = convert_array<int16_t, ComponentType>((int16_t*)input, size_bytes, n_values, normalized);
            size_component = 2;
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            converted_array = convert_array<uint16_t, ComponentType>((uint16_t*)input, size_bytes, n_values, normalized);
            size_component = 2;
            break;
        case TINYGLTF_COMPONENT_TYPE_INT:
            converted_array = convert_array<int32_t, ComponentType>((int32_t*)input, size_bytes, n_values, normalized);
            size_component = 4;
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            converted_array = convert_array<uint32_t, ComponentType>((uint32_t*)input, size_bytes, n_values, normalized);
            size_component = 4;
            break;
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            converted_array = convert_array<float, ComponentType>((float*)input, size_bytes, n_values, normalized);
            size_component = 4;
            break;
        case TINYGLTF_COMPONENT_TYPE_DOUBLE:
            converted_array = convert_array<double, ComponentType>((double*)input, size_bytes, n_values, normalized);
            size_component = 8;
            break;
        default:
            std::invalid_argument("Unsupported component type");
        }

        const size_t n_desired_components = sizeof(Out) / sizeof(ComponentType);
        const size_t n_actual_components = component_stride / size_component;

        // Example: we want vec3, and yay we actually have vec3! Just cast and return
        if (n_desired_components == n_actual_components) {
            Out* output_array = (Out*)converted_array;
            return std::vector<Out>(output_array, output_array + (n_values / n_desired_components));
        }

        // Example: we want vec4, but we only got vec3. Let's extend it using the default value
        else if (n_desired_components > n_actual_components) {
            std::vector<Out> output;
            const size_t n_extra_components = n_desired_components - n_actual_components;

            for (size_t i = 0; i < n_values; ++i) {
                // Copy the entire default value into the output, then copy the actual value into it, and then add it to the output
                Out out{};
                ComponentType* raw_components = &converted_array[i * n_actual_components];
                memcpy(&out, &default_value, sizeof(Out));
                memcpy(&out, raw_components, component_stride);
                output.push_back(out);
            }
        }

        return std::vector<Out>();
    }

    std::vector<Vertex> parse_primitive(tinygltf::Primitive& primitive, tinygltf::Model& model, const std::string& path);

    void traverse_nodes(Device& device, std::vector<int>& node_indices, tinygltf::Model& model, glm::mat4 local_transform, SceneNode* parent, const std::string& path, int depth = 0) {
        // Get all child nodes
        for (auto& node_index : node_indices) {
            auto& node = model.nodes[node_index];
            for (int i = 0; i < depth; ++i) printf("\t");
            printf("node: %s\n", node.name.c_str());

            // Convert matrix in gltf model to glm::mat4. If the matrix doesn't exist, just set it to identity matrix
            glm::mat4 local_matrix(1.0f);
            int i = 0;
            for (const auto& value : node.matrix) { local_matrix[i / 4][i % 4] = static_cast<float>(value); i++; }
            glm::mat4 global_matrix = local_transform * local_matrix;

            // Make a child node 
            auto scene_node = std::make_shared<SceneNode>();
            scene_node->name = node.name;
            scene_node->cached_global_transform = global_matrix;

            if (node.translation.size() > 0) {
                scene_node->local_transform.position = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
            }
            if (node.scale.size() > 0) {
                scene_node->local_transform.scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
            }
            if (node.rotation.size() > 0) {
                scene_node->local_transform.rotation = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
            }

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

                    // Get the vertices, as well as a separate positions buffer, which we'll use to build ray tracing acceleration structures
                    std::vector<Vertex> vertices = parse_primitive(primitive, model, path);
                    std::vector<glm::vec3> positions;
                    positions.reserve(vertices.size());
                    for (Vertex& vertex : vertices) {
                        positions.push_back(vertex.position);
                    }

                    // Generate index buffer
                    std::vector<uint32_t> indices(vertices.size());
                    for (uint32_t i = 0; i < indices.size(); ++i) {
                        indices[i] = i;
                    }

                    // Create buffers for them
                    ResourceHandlePair vertex_buffer = device.create_buffer("Vertex buffer", vertices.size() * sizeof(vertices[0]), vertices.data());
                    ResourceHandlePair position_buffer = device.create_buffer("Position buffer", positions.size() * sizeof(positions[0]), positions.data());
                    ResourceHandlePair index_buffer = device.create_buffer("Index buffer", vertices.size() * sizeof(vertices[0]), vertices.data());

                    auto mesh_node = std::make_shared<SceneNode>();
                    mesh_node->type = SceneNodeType::Mesh;
                    mesh_node->name = mesh.name;
                    mesh_node->mesh.vertex_buffer = vertex_buffer.handle;
                    mesh_node->mesh.position_buffer = position_buffer.handle;
                    // todo: D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC
                    scene_node->add_child_node(mesh_node);
                }
            }

            // If it has a light, process it
            if (node.light != -1) {
                auto& light = model.lights[node.light];
                for (int i = -1; i < depth; ++i) printf("\t");
                printf("%s light: %s\n", light.type.c_str(), light.name.c_str());
                auto light_node = std::make_shared<SceneNode>();
                light_node->type = SceneNodeType::Light;
                light_node->name = light.name;
                scene_node->add_child_node(light_node);
            }

            // If it has children, process those
            if (!node.children.empty())
            {
                traverse_nodes(device, node.children, model, local_matrix, scene_node.get(), path, depth + 1);
            }
            parent->add_child_node(scene_node);
        }
    }

    SceneNode* create_scene_graph_from_gltf(Device& device, const std::string& path)
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

        auto scene_node = new SceneNode();
        traverse_nodes(device, scene.nodes, model, glm::mat4(1.0f), scene_node, path);
        return scene_node;
    }

    std::vector<Vertex> parse_primitive(tinygltf::Primitive& primitive, tinygltf::Model& model, const std::string& path) {
        //Accessors
        int acc_position = -1;
        int acc_normal = -1;
        int acc_tangent = -1;
        int acc_tex_coord = -1;
        int acc_color = -1;
        int acc_indices = -1;

        auto attributes_to_check = { "POSITION", "NORMAL", "TANGENT", "TEXCOORD_0", "COLOR_0" };
        for (auto& attr : attributes_to_check) {
            if (!primitive.attributes.contains(attr)) {
                printf("[ERR/GLTF] Failed to parse file \"%s\": missing attribute \"%s\"\n", path.c_str(), attr);
            }
        }

        acc_position = primitive.attributes["POSITION"];
        acc_normal = primitive.attributes["NORMAL"];
        acc_tangent = primitive.attributes["TANGENT"];
        acc_tex_coord = primitive.attributes["TEXCOORD_0"];
        acc_color = primitive.attributes["COLOR_0"];
        acc_indices = primitive.indices;

        // Get array sizes
        const size_t size_position = model.bufferViews[acc_position].byteLength;
        const size_t size_normal = model.bufferViews[acc_normal].byteLength;
        const size_t size_tangent = model.bufferViews[acc_tangent].byteLength;
        const size_t size_color = model.bufferViews[acc_color].byteLength;
        const size_t size_tex_coord = model.bufferViews[acc_tex_coord].byteLength;
        const size_t size_indices = model.bufferViews[acc_indices].byteLength;

        // Prepare buffer data pointers
        uint8_t* start_position = &model.buffers[model.bufferViews[acc_position].buffer].data[model.bufferViews[acc_position].byteOffset];
        uint8_t* start_normal = &model.buffers[model.bufferViews[acc_normal].buffer].data[model.bufferViews[acc_normal].byteOffset];
        uint8_t* start_tangent = &model.buffers[model.bufferViews[acc_tangent].buffer].data[model.bufferViews[acc_tangent].byteOffset];
        uint8_t* start_color = &model.buffers[model.bufferViews[acc_color].buffer].data[model.bufferViews[acc_color].byteOffset];
        uint8_t* start_tex_coord = &model.buffers[model.bufferViews[acc_tex_coord].buffer].data[model.bufferViews[acc_tex_coord].byteOffset];
        uint8_t* start_indices = &model.buffers[model.bufferViews[acc_indices].buffer].data[model.bufferViews[acc_indices].byteOffset];

        // Get component types
        int type_position = model.accessors[acc_position].componentType;
        int type_normal = model.accessors[acc_normal].componentType;
        int type_tangent = model.accessors[acc_tangent].componentType;
        int type_color = model.accessors[acc_color].componentType;
        int type_tex_coord = model.accessors[acc_tex_coord].componentType;
        int type_indices = model.accessors[acc_indices].componentType;

        // Get component strides
        size_t stride_position = size_position / model.accessors[acc_position].count;
        size_t stride_normal = size_normal / model.accessors[acc_normal].count;
        size_t stride_tangent = size_tangent / model.accessors[acc_tangent].count;
        size_t stride_color = size_color / model.accessors[acc_color].count;
        size_t stride_tex_coord = size_tex_coord / model.accessors[acc_tex_coord].count;
        size_t stride_indices = size_indices / model.accessors[acc_indices].count;

        // Default values
        glm::vec3 default_position = glm::vec3(0.0f, 0.0f, 0.0f);
        glm::vec3 default_normal = glm::vec3(0.0f, 1.0f, 0.0f); // todo: actually calculate these
        glm::vec4 default_tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); // todo: actually calculate these
        glm::vec4 default_color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        glm::vec2 default_tex_coord = glm::vec2(0.0f, 0.0f);
        uint32_t default_index = 0;

        // Get normalized
        bool norm_color = model.accessors[acc_color].normalized;
        bool norm_tex_coord = model.accessors[acc_tex_coord].normalized;

        // Convert attributes to desired format
        auto positions = convert_gltf_buffer<float, glm::vec3>((void*)start_position, type_position, stride_position, default_position, size_position, false);
        auto normals = convert_gltf_buffer<float, glm::vec3>((void*)start_normal, type_normal, stride_normal, default_normal, size_normal, false);
        auto tangents = convert_gltf_buffer<float, glm::vec4>((void*)start_tangent, type_tangent, stride_tangent, default_tangent, size_tangent, false);
        auto colors = convert_gltf_buffer<float, glm::vec4>((void*)start_color, type_color, stride_color, default_color, size_color, norm_color);
        auto tex_coords = convert_gltf_buffer<float, glm::vec2>((void*)start_tex_coord, type_tex_coord, stride_tex_coord, default_tex_coord, size_tex_coord, norm_tex_coord);
        auto indices = convert_gltf_buffer<uint32_t, uint32_t>((void*)start_indices, type_indices, stride_indices, default_index, size_indices, false);

        // Convert to custom vertex format
        std::vector<Vertex> vertices;
        vertices.reserve(indices.size());

        for (uint32_t i : indices) {
            vertices.push_back(Vertex {
                .position = positions[i],
                .normal = normals[i],
                .tangent = tangents[i],
                .color = colors[i],
                .texcoord0 = tex_coords[i],
                }
            );
        }

        return vertices;
    }
}