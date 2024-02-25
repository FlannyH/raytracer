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

namespace gfx {
    unsigned char* load_image(char const* filename, int* x, int* y, int* comp, int req_comp) {
        stbi__vertically_flip_on_load = 1;
        return stbi_load(filename, x, y, comp, req_comp);
    }

    T* 

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
                    int size_position = model.bufferViews[acc_position].byteLength;
                    int size_normal = model.bufferViews[acc_normal].byteLength;
                    int size_tangent = model.bufferViews[acc_tangent].byteLength;
                    int size_color = model.bufferViews[acc_color].byteLength;
                    int size_tex_coords = model.bufferViews[acc_tex_coord].byteLength;
                    int size_indices = model.bufferViews[acc_indices].byteLength;

                    // Prepare buffer data pointers
                    uint8_t* start_position = &model.buffers[model.bufferViews[acc_position].buffer].data[model.bufferViews[acc_position].byteOffset];
                    uint8_t* start_normal = &model.buffers[model.bufferViews[acc_normal].buffer].data[model.bufferViews[acc_normal].byteOffset];
                    uint8_t* start_tangent = &model.buffers[model.bufferViews[acc_tangent].buffer].data[model.bufferViews[acc_tangent].byteOffset];
                    uint8_t* start_color = &model.buffers[model.bufferViews[acc_color].buffer].data[model.bufferViews[acc_color].byteOffset];
                    uint8_t* start_tex_coords = &model.buffers[model.bufferViews[acc_tex_coord].buffer].data[model.bufferViews[acc_tex_coord].byteOffset];
                    uint8_t* start_indices = &model.buffers[model.bufferViews[acc_indices].buffer].data[model.bufferViews[acc_indices].byteOffset];

                    // Get indices
                    std::vector<uint32_t> indices;
                    switch (model.accessors[acc_indices].componentType) {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        size_t n_indices = size_indices / sizeof(uint32_t);
                        uint32_t* indices_u32 = (uint32_t*)start_indices;
                        indices.resize(n_indices);
                        for (size_t i = 0; i < n_indices; ++i) {
                            indices[i] = indices_u32[i];
                        }
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        size_t n_indices = size_indices / sizeof(uint16_t);
                        uint16_t* indices_u16 = (uint16_t*)start_indices;
                        indices.resize(n_indices);
                        for (size_t i = 0; i < n_indices; ++i) {
                            indices[i] = indices_u16[i];
                        }
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        size_t n_indices = size_indices / sizeof(uint8_t);
                        uint8_t* indices_u8 = (uint8_t*)start_indices;
                        indices.resize(n_indices);
                        for (size_t i = 0; i < n_indices; ++i) {
                            indices[i] = indices_u8[i];
                        }
                        break;
                    }

                    // Convert to floats
                    switch (model.accessors[acc_position].componentType) {
                    case TINYGLTF_COMPONENT_TYPE_FLOAT:
                        size_t n_indices = size_indices / sizeof(float);
                        float* indices = (float*)start_indices;
                        indices.resize(n_indices);
                        for (size_t i = 0; i < n_indices; ++i) {
                            indices[i] = indices_u8[i];
                        }
                    default:
                        break;
                    }

                    // Convert to custom vertex format
                    //mesh.vertex_buffer.reserve(mesh.size_position + mesh.size_normal + mesh.size_tangent + mesh.size_color + mesh.size_tex_coords);
                    //mesh.index_buffer.reserve(mesh.size_indices);
                    //for (int i = 0; i < mesh.size_position; i++) { mesh.vertex_buffer.push_back(start_position[i]); }
                    //for (int i = 0; i < mesh.size_normal; i++) { mesh.vertex_buffer.push_back(start_normal[i]); }
                    //for (int i = 0; i < mesh.size_tangent; i++) { mesh.vertex_buffer.push_back(start_tangent[i]); }
                    //for (int i = 0; i < mesh.size_color; i++) { mesh.vertex_buffer.push_back(start_color[i]); }
                    //for (int i = 0; i < mesh.size_tex_coords; i++) { mesh.vertex_buffer.push_back(start_tex_coords[i]); }
                    //for (int i = 0; i < mesh.size_indices; i++) { mesh.index_buffer.push_back(start_indices[i]); }

                    //// Get index type
                    //switch (model.accessors[acc_indices].componentType)
                    //{
                    //case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    //    mesh.index_attr_type = GL_UNSIGNED_SHORT;
                    //    break;
                    //case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    //    mesh.index_attr_type = GL_UNSIGNED_INT;
                    //    break;
                    //default:
                    //    printf("[ERR] unexpected index format! %i\n", model.accessors[acc_indices].componentType);
                    //    break;
                    //}

                    ////Get transform
                    //mesh.local_model_matrix = glm::mat4(1.0f);
                    //if (node.scale.size() > 0)
                    //{
                    //    mesh.local_model_matrix = glm::scale(mesh.local_model_matrix, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
                    //}
                    //if (node.rotation.size() > 0)
                    //{
                    //    mesh.local_model_matrix = glm::toMat4(glm::quat(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3])) * mesh.local_model_matrix;
                    //}
                    //if (node.translation.size() > 0)
                    //{
                    //    mesh.local_model_matrix = glm::translate(mesh.local_model_matrix, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
                    //}

                    //output.push_back(mesh);
                }
            }
        }

        return loadSuccess;

    }
}