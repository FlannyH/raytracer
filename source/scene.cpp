#include "scene.h"
#include <glm/glm.hpp>
#include <limits>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define JSON_NOEXCEPTION

#pragma warning(push)
#pragma warning(disable: 4018)
#pragma warning(disable: 4267)
#include <tinygltf/tiny_gltf.h>
#pragma warning(pop)
#include "tangent.h"

namespace gfx {
    glm::mat4 Transform::as_matrix() {
        glm::mat4 mat_translate = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 mat_rotate = glm::toMat4(rotation);
        glm::mat4 mat_scale = glm::scale(glm::mat4(1.0f), scale);
        return mat_translate * mat_rotate * mat_scale;
    }

    glm::mat4 Transform::as_view_matrix() {
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

#pragma warning(push)
#pragma warning(disable: 4723) // Visual Studio is somehow convinced that dividing by max_ can cause divide by zero. I literally check for that in the if statement, so Visual Studio is tripping sack.
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
            if (normalized && max_ != 0) {
                output[i] = std::clamp(((Out)input[i]) / max_, min_, max_);
            }
            else { 
                output[i] = (Out)input[i]; 
            }
        }
        n_values = n_values_;
        return output;
    }
#pragma warning(pop)

    /// Takes the input array, casts all entries to ComponentType, converts it to ComponentType, and returns a vector of the Out type
    /// `Out` is expected to be a floating point type such as `float`, `glm::mat4`, `glm::vec3`, etc
    /// `ComponentType` is the component type of `Out`, so if `Out` is a `glm::vec3`, the `ComponentType` should be `float`
    /// `default_value` is used in case we want more components than we have, so then the last values will take the default value's component
    template<typename ComponentType, typename Out>
    std::vector<Out> convert_gltf_buffer(void* input, int component_type, size_t component_stride, Out default_value, size_t size_bytes, const bool normalized) {
        if (input == nullptr || size_bytes == 0) return {};
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
            std::abort(); // Unsupported component type
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

            for (size_t i = 0; i < n_values / n_actual_components; ++i) {
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

    void traverse_nodes(Renderer& renderer, std::vector<int>& node_indices, tinygltf::Model& model, glm::mat4 local_transform, SceneNode* parent, const std::string& path, const std::vector<int>& material_mapping, int depth = 0) {
        // Get all child nodes
        for (auto& node_index : node_indices) {
            auto& node = model.nodes[node_index];

            // Convert matrix in gltf model to glm::mat4. If the matrix doesn't exist, just set it to identity matrix
            glm::mat4 local_matrix(1.0f);
            int i = 0;
            if (node.matrix.empty()) {
                Transform local_transform;
                if (node.translation.empty() == false) {
                    local_transform.position = glm::vec3(
                        (float)node.translation[0],
                        (float)node.translation[1],
                        (float)node.translation[2]
                    );
                }
                if (node.rotation.empty() == false) {
                    local_transform.rotation = glm::quat(
                        (float)node.rotation[3],
                        (float)node.rotation[0],
                        (float)node.rotation[1],
                        (float)node.rotation[2]
                    );
                }
                if (node.scale.empty() == false) {
                    local_transform.scale = glm::vec3(
                        (float)node.scale[0],
                        (float)node.scale[1],
                        (float)node.scale[2]
                    );
                }
                
                local_matrix = local_transform.as_matrix();
            }
            else {
                for (const auto& value : node.matrix) { local_matrix[i / 4][i % 4] = static_cast<float>(value); i++; }
            }
            glm::mat4 global_matrix = local_transform * local_matrix;

            // Make a child node 
            auto scene_node = std::make_shared<SceneNode>(SceneNodeType::empty);
            scene_node->name = node.name;
            scene_node->cached_global_transform = global_matrix;

            // If it has a mesh, process it
            if (node.mesh != -1) {
                auto& mesh = model.meshes[node.mesh];
                auto& primitives = mesh.primitives;
                for (auto& primitive : primitives) {
                    // Get the vertices, as well as a separate positions buffer, which we'll use to build ray tracing acceleration structures
                    std::vector<Vertex> vertices = parse_primitive(primitive, model, path);
                    std::vector<VertexCompressed> compressed_vertices;
                    std::vector<glm::vec3> positions;
                    positions.reserve(vertices.size());
                    compressed_vertices.reserve(vertices.size());

                    // Populate position buffer
                    for (const Vertex& vertex : vertices) {
                        positions.push_back(vertex.position);
                    }

                    // Figure out vertex position range for the same of compression
                    glm::vec3 min_position = glm::vec3(+INFINITY);
                    glm::vec3 max_position = glm::vec3(-INFINITY);
                    for (const Vertex& vertex : vertices) {
                        min_position = glm::min(min_position, vertex.position);
                        max_position = glm::max(max_position, vertex.position);
                    }

                    // Some helper values to remap from (min, max) to (0, 65535)
                    const glm::vec3 offset = min_position;
                    const glm::vec3 scale = max_position - min_position;

                    // Compress vertices for raster pipeline
                    for (const Vertex& vertex : vertices) {
                        compressed_vertices.emplace_back(VertexCompressed{
                            .position = glm::clamp(glm::u16vec3((vertex.position - offset) * (65535.0f / scale)), glm::u16vec3(0), glm::u16vec3(65535)),    // remap from (min, max) to (0, 65535)
                            .material_id = (uint16_t)((primitive.material == -1) ? 0xFFFF : material_mapping.at(primitive.material)),
                            .normal = glm::u8vec3((vertex.normal + 1.0f) * 127.0f),                       // remap from (-1, +1) to (0, 254)
                            .flags1 = {
                                .tangent_sign = (uint8_t)((vertex.tangent.w > 0.0f) ? 1 : 0),             // convert tangent sign to a single bit
                            },
                            .tangent = glm::u8vec3((glm::vec3(vertex.tangent) + 1.0f) * 127.0f),          // remap from (-1, +1) to (0, 254)
                            .color = glm::u16vec4(vertex.color * 1023.0f),                                // remap from (0.0, 1.0) to (0, 1023). for RGB, higher values are valid too
                            .texcoord0 = vertex.texcoord0,                                                // keep this one the same
                        });
                    }

                    // Generate index buffer
                    std::vector<uint32_t> indices(vertices.size());
                    for (uint32_t i = 0; i < indices.size(); ++i) {
                        indices[i] = i;
                    }

                    // Create buffers for them
                    ResourceHandlePair vertex_buffer = renderer.create_buffer(node.name + " (compressed vertex buffer)", compressed_vertices.size() * sizeof(compressed_vertices[0]), compressed_vertices.data(), ResourceUsage::non_pixel_shader_read);
                    ResourceHandlePair index_buffer = renderer.create_buffer(node.name + " (index buffer)", indices.size() * sizeof(indices[0]), indices.data(), ResourceUsage::non_pixel_shader_read);

                    auto mesh_node = std::make_shared<SceneNode>(SceneNodeType::mesh);
                    
                    if (renderer.supports(RendererFeature::raytracing)) {
                        // Create geometry
                        ResourceHandlePair position_buffer = renderer.create_buffer(node.name + " (position buffer)", positions.size() * sizeof(positions[0]), positions.data(), ResourceUsage::non_pixel_shader_read);
                        ResourceHandlePair blas = renderer.create_blas(node.name, position_buffer, index_buffer, vertices.size(), indices.size());
                        
                        mesh_node->expect_mesh().position_buffer = position_buffer.handle;
                        mesh_node->expect_mesh().blas = blas;
                    }

                    mesh_node->type = SceneNodeType::mesh;
                    mesh_node->name = mesh.name;
                    mesh_node->cached_global_transform = global_matrix;
                    mesh_node->position_offset = offset;
                    mesh_node->position_scale = scale;
                    mesh_node->expect_mesh().vertex_buffer = vertex_buffer.handle;
                    scene_node->add_child_node(mesh_node);
                }
            }

            // If it has a light, process it
            if (node.light != -1) {
                const auto& light = model.lights[node.light];
                auto light_node = std::make_shared<SceneNode>(SceneNodeType::light);
                light_node->type = SceneNodeType::light;
                light_node->name = light.name;
                if (light.color.size() >= 3) {
                    light_node->expect_light().color.r = (float)light.color[0];
                    light_node->expect_light().color.g = (float)light.color[1];
                    light_node->expect_light().color.b = (float)light.color[2];
                }
                light_node->expect_light().intensity = (float)light.intensity;
                if (light.type == "directional") {
                    light_node->expect_light().type = LightType::Directional;
                }
                light_node->cached_global_transform = global_matrix;
                scene_node->add_child_node(light_node);
            }

            // If it has children, process those
            if (!node.children.empty()) {
                traverse_nodes(renderer, node.children, model, global_matrix, scene_node.get(), path, material_mapping, depth + 1);
            }
            parent->add_child_node(scene_node);
        }
    }

    static PixelFormat pixel_format_from_gltf_image(const tinygltf::Image& image) {
        if (image.component == 1 && image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) return PixelFormat::r8_unorm;
        if (image.component == 2 && image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) return PixelFormat::rg8_unorm;
        if (image.component == 4 && image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) return PixelFormat::rgba8_unorm;
        else {
            LOG(Error, "Unknown/unsupported pixel type in glTF image!");
            return PixelFormat::none;
        }
    }

    ResourceHandlePair upload_texture_from_gltf(const std::string& model_path, tinygltf::Model& model, Renderer& renderer, int texture_index) {
        ResourceHandlePair texture_resource;
        const tinygltf::Texture* texture_gltf = (texture_index != -1) ? &model.textures.at(texture_index) : nullptr;
        const tinygltf::Image* image_gltf = (texture_index != -1) ? &model.images.at(texture_gltf->source) : nullptr;
        // todo: deduplicate textures

        if (!image_gltf) {
            return ResourceHandlePair();
        }

        // If the image is embedded, `uri` is empty and `image` is populated, so create a file name and use the `image` data
        else if (image_gltf->uri.empty()) {
            const std::string texture_path = model_path + "::" + image_gltf->name;
            LOG(Debug, "Loading embedded image: %s", texture_path.c_str());
            return renderer.load_texture(
                texture_path,
                (uint32_t)image_gltf->width,
                (uint32_t)image_gltf->height,
                1,
                (void*)image_gltf->image.data(),
                pixel_format_from_gltf_image(*image_gltf),
                TextureType::tex_2d,
                ResourceUsage::compute_write, // in case we want to do any corrections
                true
            );
        }
        
        // If the image is external, `uri` is populated and `image` is empty, so load the image from disk
        LOG(Debug, "Loading external image: %s", image_gltf->uri.c_str());
        const std::string texture_path = model_path.substr(0, model_path.find_last_of('/') + 1) + image_gltf->uri;
        return renderer.load_texture(texture_path);
    }

    void get_rt_instances_from_scene_nodes(SceneNode* node, std::vector<RaytracingInstance>& instances) {
        if (node->type == SceneNodeType::mesh) {
            // If we set the instance id to the vertex buffer handle, we can fetch this in the shader
            // later using CommittedInstanceID(), and then combine it with CandidatePrimitiveIndex() 
            // to fetch that triangle's data for shading.
            instances.emplace_back(RaytracingInstance {
                .transform = glm::mat4x3(node->cached_global_transform),
                .instance_id = node->expect_mesh().vertex_buffer.id,
                .instance_mask = 0xFF,
                .instance_contribution_to_hitgroup_index = 0,
                .flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE,
                .blas = node->expect_mesh().blas
            });
            static_assert(D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE == VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR);
        }
        for (const auto& child : node->children) {
            get_rt_instances_from_scene_nodes(child.get(), instances);
        }
    }

    SceneNode* create_scene_graph_from_gltf(Renderer& renderer, const std::string& path) {
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

        if (model.scenes.empty()) {
            LOG(Warning, "Empty model or failed to load file '%s'!", path.c_str());
            return nullptr;
        }

        // Parse materials
        std::vector<int> material_mapping;
        for (auto& model_material : model.materials) {
            // Allocate slot
            auto alloc_mat_slot = renderer.allocate_material_slot();
            material_mapping.push_back(alloc_mat_slot.first);

            // Figure out what textures this material has and load them
            auto color_texture = upload_texture_from_gltf(path, model, renderer, model_material.pbrMetallicRoughness.baseColorTexture.index);
            auto normal_texture = upload_texture_from_gltf(path, model, renderer, model_material.normalTexture.index);
            auto metal_roughness_texture = upload_texture_from_gltf(path, model, renderer, model_material.pbrMetallicRoughness.metallicRoughnessTexture.index);
            auto emissive_texture = upload_texture_from_gltf(path, model, renderer, model_material.emissiveTexture.index);

            if (normal_texture.handle.is_loaded) renderer.reconstruct_normal_map(normal_texture);

            renderer.generate_mipmaps(color_texture);
            renderer.generate_mipmaps(normal_texture);
            renderer.generate_mipmaps(metal_roughness_texture);
            renderer.generate_mipmaps(emissive_texture);

            // Populate material struct
            if (model_material.pbrMetallicRoughness.baseColorFactor.size() == 4) {
                alloc_mat_slot.second->color_multiplier.r = (float)model_material.pbrMetallicRoughness.baseColorFactor[0];
                alloc_mat_slot.second->color_multiplier.g = (float)model_material.pbrMetallicRoughness.baseColorFactor[1];
                alloc_mat_slot.second->color_multiplier.b = (float)model_material.pbrMetallicRoughness.baseColorFactor[2];
                alloc_mat_slot.second->color_multiplier.a = (float)model_material.pbrMetallicRoughness.baseColorFactor[3];
            }
            if (model_material.emissiveFactor.size() == 3) {
                alloc_mat_slot.second->emissive_multiplier.r = (float)model_material.emissiveFactor[0];
                alloc_mat_slot.second->emissive_multiplier.g = (float)model_material.emissiveFactor[1];
                alloc_mat_slot.second->emissive_multiplier.b = (float)model_material.emissiveFactor[2];
            }
            alloc_mat_slot.second->color_texture = color_texture.handle;
            alloc_mat_slot.second->normal_texture = normal_texture.handle;
            alloc_mat_slot.second->metal_roughness_texture = metal_roughness_texture.handle;
            alloc_mat_slot.second->emissive_texture = emissive_texture.handle;
            alloc_mat_slot.second->normal_intensity = 1.0f;
            alloc_mat_slot.second->roughness_multiplier = 1.0f;
            alloc_mat_slot.second->metallic_multiplier = 1.0f;
        }

        // Find default scene and create a scene graph from it
        auto scene_to_load = (model.defaultScene != -1) ? model.defaultScene : 0;
        auto& scene = model.scenes[model.defaultScene];
        LOG(Info, "Loading scene \"%s\" from file \"%s\"", scene.name.c_str(), path.c_str());

        auto scene_node = new SceneNode(SceneNodeType::root);
        traverse_nodes(renderer, scene.nodes, model, glm::mat4(1.0f), scene_node, path, material_mapping);

        if (renderer.supports(RendererFeature::raytracing)) {
            std::vector<RaytracingInstance> instances;
            get_rt_instances_from_scene_nodes(scene_node, instances);

            if (!instances.empty()) {
                auto tlas = renderer.create_tlas(scene.name.c_str(), instances);
                scene_node->expect_root() = { .tlas = tlas };
            }
            else {
                scene_node->expect_root() = { .tlas = {} };
            }
        }

        return scene_node;
    }

    constexpr int size_of_gltf_component(const int gltf_component) {
        switch (gltf_component) {
            case TINYGLTF_COMPONENT_TYPE_BYTE: return 1;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return 1;
            case TINYGLTF_COMPONENT_TYPE_SHORT: return 2;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return 2;
            case TINYGLTF_COMPONENT_TYPE_INT: return 4;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: return 4;
            case TINYGLTF_COMPONENT_TYPE_FLOAT: return 4;
            case TINYGLTF_COMPONENT_TYPE_DOUBLE: return 8;
            default: abort();
        }
    }

    constexpr int number_of_components(const int gltf_type) {
        switch (gltf_type) {
        case TINYGLTF_TYPE_VEC2: return 2;
        case TINYGLTF_TYPE_VEC3: return 3;
        case TINYGLTF_TYPE_VEC4: return 4;
        case TINYGLTF_TYPE_MAT2: return 4;
        case TINYGLTF_TYPE_MAT3: return 9;
        case TINYGLTF_TYPE_MAT4: return 16;
        case TINYGLTF_TYPE_SCALAR: return 1;
        default: abort();
        }
    }

    std::vector<Vertex> parse_primitive(tinygltf::Primitive& primitive, tinygltf::Model& model, const std::string& path) {
        //Accessors
        int acc_position = -1;
        int acc_normal = -1;
        int acc_tangent = -1;
        int acc_tex_coord = -1;
        int acc_color = -1;
        int acc_indices = -1;

        auto attributes_to_check = { "POSITION" };
        for (auto& attr : attributes_to_check) {
            if (!primitive.attributes.contains(attr)) {
                LOG(Error, "Failed to parse glTF file \"%s\": missing attribute \"%s\"", path.c_str(), attr);
            }
        }

        if (primitive.attributes.contains("POSITION")) acc_position = primitive.attributes["POSITION"];
        if (primitive.attributes.contains("NORMAL")) acc_normal = primitive.attributes["NORMAL"];
        if (primitive.attributes.contains("TANGENT")) acc_tangent = primitive.attributes["TANGENT"];
        if (primitive.attributes.contains("TEXCOORD_0")) acc_tex_coord = primitive.attributes["TEXCOORD_0"];
        if (primitive.attributes.contains("COLOR_0")) acc_color = primitive.attributes["COLOR_0"];
        acc_indices = primitive.indices;

        // Get bufferviews
        const auto* bufferview_position = (acc_position == -1) ? nullptr : &model.bufferViews[model.accessors[acc_position].bufferView];
        const auto* bufferview_normal = (acc_normal == -1) ? nullptr : &model.bufferViews[model.accessors[acc_normal].bufferView];
        const auto* bufferview_tangent = (acc_tangent == -1) ? nullptr : &model.bufferViews[model.accessors[acc_tangent].bufferView];
        const auto* bufferview_color = (acc_color == -1) ? nullptr : &model.bufferViews[model.accessors[acc_color].bufferView];
        const auto* bufferview_tex_coord = (acc_tex_coord == -1) ? nullptr : &model.bufferViews[model.accessors[acc_tex_coord].bufferView];
        const auto* bufferview_indices = (acc_indices == -1) ? nullptr : &model.bufferViews[model.accessors[acc_indices].bufferView];

        // Prepare buffer data pointers
        uint8_t* start_position = (acc_position == -1) ? nullptr : &model.buffers[bufferview_position->buffer].data[bufferview_position->byteOffset + model.accessors[acc_position].byteOffset];
        uint8_t* start_normal = (acc_normal == -1) ? nullptr : &model.buffers[bufferview_normal->buffer].data[bufferview_normal->byteOffset + model.accessors[acc_normal].byteOffset];
        uint8_t* start_tangent = (acc_tangent == -1) ? nullptr : &model.buffers[bufferview_tangent->buffer].data[bufferview_tangent->byteOffset + model.accessors[acc_tangent].byteOffset];
        uint8_t* start_color = (acc_color == -1) ? nullptr : &model.buffers[bufferview_color->buffer].data[bufferview_color->byteOffset + model.accessors[acc_color].byteOffset];
        uint8_t* start_tex_coord = (acc_tex_coord == -1) ? nullptr : &model.buffers[bufferview_tex_coord->buffer].data[bufferview_tex_coord->byteOffset + model.accessors[acc_tex_coord].byteOffset];
        uint8_t* start_indices = (acc_indices == -1) ? nullptr : &model.buffers[bufferview_indices->buffer].data[bufferview_indices->byteOffset + model.accessors[acc_indices].byteOffset];

        // Get component types
        int type_position = (acc_position == -1) ? 0 : model.accessors[acc_position].componentType;
        int type_normal = (acc_normal == -1) ? 0 : model.accessors[acc_normal].componentType;
        int type_tangent = (acc_tangent == -1) ? 0 : model.accessors[acc_tangent].componentType;
        int type_color = (acc_color == -1) ? 0 : model.accessors[acc_color].componentType;
        int type_tex_coord = (acc_tex_coord == -1) ? 0 : model.accessors[acc_tex_coord].componentType;
        int type_indices = (acc_indices == -1) ? 0 : model.accessors[acc_indices].componentType;

        // Get component strides
        size_t stride_position =  (acc_position == -1) ? 0 : ((bufferview_position->byteStride == 0) ? (size_of_gltf_component(type_position) * number_of_components(model.accessors[acc_position].type)) : bufferview_position->byteStride);
        size_t stride_normal = (acc_normal == -1) ? 0 : ((bufferview_normal->byteStride == 0) ? (size_of_gltf_component(type_normal) * number_of_components(model.accessors[acc_normal].type)) : bufferview_normal->byteStride);
        size_t stride_tangent = (acc_tangent == -1) ? 0 : ((bufferview_tangent->byteStride == 0) ? (size_of_gltf_component(type_tangent) * number_of_components(model.accessors[acc_tangent].type)) : bufferview_tangent->byteStride);
        size_t stride_color = (acc_color == -1) ? 0 : ((bufferview_color->byteStride == 0) ? (size_of_gltf_component(type_color) * number_of_components(model.accessors[acc_color].type)) : bufferview_color->byteStride);
        size_t stride_tex_coord = (acc_tex_coord == -1) ? 0 : ((bufferview_tex_coord->byteStride == 0) ? (size_of_gltf_component(type_tex_coord) * number_of_components(model.accessors[acc_tex_coord].type)) : bufferview_tex_coord->byteStride);
        size_t stride_indices = (acc_indices == -1) ? 0 : ((bufferview_indices->byteStride == 0)    ? (size_of_gltf_component(type_indices)     * number_of_components(model.accessors[acc_indices].type)) : bufferview_indices->byteStride);

        // Get array sizes
        const size_t size_position = (acc_position == -1) ? 0 : model.accessors[acc_position].count * stride_position;
        const size_t size_normal = (acc_normal == -1) ? 0 : model.accessors[acc_normal].count * stride_normal;
        const size_t size_tangent = (acc_tangent == -1) ? 0 : model.accessors[acc_tangent].count * stride_tangent;
        const size_t size_color = (acc_color == -1) ? 0 : model.accessors[acc_color].count * stride_color;
        const size_t size_tex_coord = (acc_tex_coord == -1) ? 0 : model.accessors[acc_tex_coord].count * stride_tex_coord;
        const size_t size_indices = (acc_indices == -1) ? 0 : model.accessors[acc_indices].count * stride_indices;

        // Default values
        glm::vec3 default_position = glm::vec3(0.0f, 0.0f, 0.0f);
        glm::vec3 default_normal = glm::vec3(0.0f, 1.0f, 0.0f); // todo: actually calculate these
        glm::vec4 default_tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); // todo: actually calculate these
        glm::vec4 default_color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        glm::vec2 default_tex_coord = glm::vec2(0.0f, 0.0f);
        uint32_t default_index = 0;

        // Get normalized
        bool norm_color = (acc_color == -1) ? false : model.accessors[acc_color].normalized;
        bool norm_tex_coord = (acc_tex_coord == -1) ? false : model.accessors[acc_tex_coord].normalized;

        // Convert attributes to desired format
        auto positions = convert_gltf_buffer<float, glm::vec3>((void*)start_position, type_position, stride_position, default_position, size_position, false);
        auto normals = convert_gltf_buffer<float, glm::vec3>((void*)start_normal, type_normal, stride_normal, default_normal, size_normal, false);
        auto tangents = convert_gltf_buffer<float, glm::vec4>((void*)start_tangent, type_tangent, stride_tangent, default_tangent, size_tangent, false);
        auto colors = convert_gltf_buffer<float, glm::vec4>((void*)start_color, type_color, stride_color, default_color, size_color, norm_color);
        auto tex_coords = convert_gltf_buffer<float, glm::vec2>((void*)start_tex_coord, type_tex_coord, stride_tex_coord, default_tex_coord, size_tex_coord, norm_tex_coord);
        auto indices = convert_gltf_buffer<uint32_t, uint32_t>((void*)start_indices, type_indices, stride_indices, default_index, size_indices, false);

        // Generate potentially missing data
        if (colors.empty()) colors.resize(positions.size(), default_color);
        if (tex_coords.empty()) tex_coords.resize(positions.size(), default_tex_coord);

        // If we don't have normals, generate flat normals (and if you want better normals, make sure your modeling software exports them)
        if (normals.empty()) {
            normals.reserve(positions.size());
            for (size_t i = 0; i < positions.size(); i += 3) {
                const glm::vec3 a = positions[i + 0];
                const glm::vec3 b = positions[i + 1];
                const glm::vec3 c = positions[i + 2];
                const glm::vec3 ab = b - a;
                const glm::vec3 ac = c - a;
                // todo: verify winding order
                normals[i] = glm::normalize(glm::cross(ab, ac));
            }
        }

        // Use MikkTSpace to generate tangents if we don't have them yet
        bool should_generate_tangents = tangents.empty();
        tangents.resize(positions.size());

        // Convert to custom vertex format
        std::vector<Vertex> vertices;
        vertices.reserve(indices.size());

        if (indices.empty()) {
            for (size_t i = 0; i < positions.size(); ++i) {
                vertices.emplace_back(Vertex{
                    .position = positions[i],
                    .normal = normals[i],
                    .tangent = tangents[i],
                    .color = colors[i],
                    .texcoord0 = tex_coords[i],
                    }
                );
            }
        }
        else {
            for (uint32_t i : indices) {
                vertices.emplace_back(Vertex{
                    .position = positions[i],
                    .normal = normals[i],
                    .tangent = tangents[i],
                    .color = colors[i],
                    .texcoord0 = tex_coords[i],
                    }
                );
            }
        }

        if (should_generate_tangents) {
            auto tangent_calculator = TangentCalculator();
            tangent_calculator.calculate_tangents(vertices.data(), vertices.size() / 3);
        }

        return vertices;
    }
}
