#include "pipeline.h"
#include "device.h"
#include "../shader.h"

namespace gfx::vk {
    Pipeline::Pipeline(const Device& device, const std::string& name, const std::string& vertex_shader_path, const std::string& pixel_shader_path, const std::vector<VkFormat> render_target_formats, const VkFormat depth_target_format) : name(name)  {
        TODO();
    }

    Pipeline::Pipeline(const Device& device, const std::string& name, const std::string& compute_shader_path) : name(name) {
        // Compile shader
        const auto cs = Shader(compute_shader_path, "main", ShaderType::compute, RenderBackend::vulkan); // todo: add customizable entry point

        // Make sure it worked
        if (cs.shader_blob.Get() == nullptr) {
            LOG(Error, "Failed to compile shader \"%s\" (%s)", name.c_str(), compute_shader_path.c_str());
            return;
        }

        create_global_root_signature(device);

        // D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline_state_desc {
        //     .pRootSignature = root_signature.Get(),
        //     .CS = {
        //         cs.shader_blob->GetBufferPointer(),
        //         cs.shader_blob->GetBufferSize()
        //     },
        // };

        // validate(device.device->CreateComputePipelineState(&pipeline_state_desc, IID_PPV_ARGS(&pipeline_state)));
        // validate(pipeline_state->SetName(L"Compute Pipeline State")); // todo: add customizable name with default parameter
    }
    
    void Pipeline::create_global_root_signature(const Device &device) {
        // Root constants
        VkPushConstantRange root_parameters[1] = {{ 
            .stageFlags = VK_SHADER_STAGE_ALL,
            .offset = 0, 
            .size = 16, 
        }};

        // Static samplers
        VkSamplerCreateInfo sampler_create_info[2] = {
            { // Regular texture
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter = VK_FILTER_LINEAR,
                .minFilter = VK_FILTER_LINEAR,
                .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .mipLodBias = 0.0f,
                .anisotropyEnable = true,
                .maxAnisotropy = 16,
                .compareEnable = false,
                .minLod = 0.0f,
                .maxLod = 100000.0f,
                .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
                .unnormalizedCoordinates = false,
            },
            { // Lookup texture & cubemap (clamp)
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter = VK_FILTER_LINEAR,
                .minFilter = VK_FILTER_LINEAR,
                .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .mipLodBias = 0.0f,
                .anisotropyEnable = 0,
                .compareEnable = false,
                .minLod = 0.0f,
                .maxLod = 100000.0f,
                .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
                .unnormalizedCoordinates = false,
            },
        };
        VkSampler samplers[countof(sampler_create_info)];

        for (size_t i = 0; i < countof(sampler_create_info); ++i) {
            vkCreateSampler(device.device, &sampler_create_info[i], nullptr, &samplers[i]);
        }

        VkDescriptorSetLayoutBinding layout_bindings[countof(sampler_create_info)] = {
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers = &samplers[0],
            },
            {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers = &samplers[1],
            }
        };

        VkDescriptorSetLayoutCreateInfo set_layout_create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
            .bindingCount = countof(layout_bindings),
            .pBindings = layout_bindings,
        };

        VkDescriptorSetLayout set_layout;
        vkCreateDescriptorSetLayout(device.device, &set_layout_create_info, nullptr, &set_layout);

        VkPipelineLayoutCreateInfo pipeline_layout_create_info { 
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &set_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = root_parameters,
        };
    }
}
