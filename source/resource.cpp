#pragma warning (push, 0)
#ifndef TINYGLTF_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#endif
#include <tinygltf/tiny_gltf.h>
#pragma warning (pop)

#include "resource.h"
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

namespace gfx {
    unsigned char* load_image(char const* filename, int* x, int* y, int* comp, int req_comp) {
        stbi__vertically_flip_on_load = 1;
        return stbi_load(filename, x, y, comp, req_comp);
    }
    
    /// Takes the input array, casts all the entries in the array to the output type, and then returns a pointer to the converted array. 
    template<typename In, typename Out>
    Out* convert_array(In* input, const size_t input_size_bytes, size_t& n_values, const float multiplier) {
        const int64_t min = std::numeric_limits<In>::min();
        const int64_t max = std::numeric_limits<In>::max();
        const size_t n_values_ = input_size_bytes / sizeof(In);
        Out* output = new Out[n_values_];
        for (size_t i = 0; i < n_values_; ++i) {
            output[i] = ((Out)input[i]) / max;
            std::clamp(output[i], min, max);
        }
        n_values = n_values_;
        return output;
    }

    /// Takes the input array, casts all entries to float, and returns a vector with those floats
    /// `Out` is expected to be a floating point type such as `float`, `glm::mat4`, `glm::vec3`, etc
    /// `default_value` is used in case we want more components than we have, so then the last values will take the default value's component
    template<typename ComponentType, typename Out>
    std::vector<Out> convert_gltf_buffer(void* input, int component_type, int component_stride, Out default_value, size_t size_bytes, bool normalized) {
        ComponentType* converted_array = nullptr;
        size_t size_component = 0;
        size_t n_values = 0;

        switch (component_type) {
        case TINYGLTF_COMPONENT_TYPE_BYTE:
            converted_array = convert_array<int8_t, ComponentType>((int8_t*)input, size_bytes, n_values);
            size_component = 1;
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            converted_array = convert_array<uint8_t, ComponentType>((uint8_t*)input, size_bytes, n_values);
            size_component = 1;
            break;
        case TINYGLTF_COMPONENT_TYPE_SHORT:
            converted_array = convert_array<int16_t, ComponentType>((int16_t*)input, size_bytes, n_values);
            size_component = 2;
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            converted_array = convert_array<uint16_t, ComponentType>((uint16_t*)input, size_bytes, n_values);
            size_component = 2;
            break;
        case TINYGLTF_COMPONENT_TYPE_INT:
            converted_array = convert_array<int32_t, ComponentType>((int32_t*)input, size_bytes, n_values);
            size_component = 4;
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            converted_array = convert_array<uint32_t, ComponentType>((uint32_t*)input, size_bytes, n_values);
            size_component = 4;
            break;
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            converted_array = convert_array<float, ComponentType>((float*)input, size_bytes, n_values);
            size_component = 4;
            break;
        case TINYGLTF_COMPONENT_TYPE_DOUBLE:
            converted_array = convert_array<double, ComponentType>((double*)input, size_bytes, n_values);
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
                Out out;
                ComponentType* raw_components = &converted_array[i * n_actual_components];
                memcpy(&out, &default_value, sizeof(Out));
                memcpy(&out, raw_components, component_stride);
                output.push_back(out);
            }
        }

        return std::vector<Out>();
    }

    bool load_gltf(const char* path)
    {
        tinygltf::TinyGLTF loader;
        tinygltf::Model model;
        std::string errors;
        std::string warnings;

        //Try to load the GLTF file
        bool loadSuccess = loader.LoadASCIIFromFile(&model, &errors, &warnings, path);

        //If something went wrong, print that to the console
        if (!warnings.empty())
        {
            printf("[WARN/GLTF] %s\n", warnings.c_str());
        }
        if (!errors.empty())
        {
            printf("[ERR/GLTF] %s\n", errors.c_str());
            return false;
        }

        //Print whether or not the file was successfully loaded
        if (!loadSuccess)
        {
            printf("[ERR/GLTF] Failed to load file \"%s\"\n", path);
            return false;
        }
        printf("[INFO/GLTF] Successfully loaded file \"%s\"\n", path);

        //Go through all the nodes, get the meshes, and add them to the mesh list
        //Scene
        for (auto& node : model.nodes)
        {
            //Node
            if (node.mesh > -1)
            {
                //Mesh
                for (auto& primitive : model.meshes[node.mesh].primitives)
                {
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
                            printf("[ERR/GLTF] Failed to parse file \"%s\": missing attribute \"%s\"\n", path, attr);
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
                    int stride_position = size_position / model.accessors[acc_position].count;
                    int stride_normal = size_normal / model.accessors[acc_normal].count;
                    int stride_tangent = size_tangent / model.accessors[acc_tangent].count;
                    int stride_color = size_color / model.accessors[acc_color].count;
                    int stride_tex_coord = size_tex_coord / model.accessors[acc_tex_coord].count;
                    int stride_indices = size_indices / model.accessors[acc_indices].count;

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
                        vertices.push_back(Vertex{
                            .position = positions[i],
                            .normal = normals[i],
                            .tangent = tangents[i],
                            .color = colors[i],
                            .texcoord0 = tex_coords[i],
                        });
                    }

                    // Get transform
                    /*mesh.local_model_matrix = glm::mat4(1.0f);
                    if (node.scale.size() > 0)
                    {
                        mesh.local_model_matrix = glm::scale(mesh.local_model_matrix, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
                    }
                    if (node.rotation.size() > 0)
                    {
                        mesh.local_model_matrix = glm::toMat4(glm::quat(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3])) * mesh.local_model_matrix;
                    }
                    if (node.translation.size() > 0)
                    {
                        mesh.local_model_matrix = glm::translate(mesh.local_model_matrix, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
                    }

                    output.push_back(mesh);*/
                    printf("yahoo\n");
                }
            }
        }

        return loadSuccess;

    }
}