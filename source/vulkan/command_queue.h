#pragma once
#include <vulkan/vulkan.h>
#include <deque>
#include <memory>

#include "../common.h"

namespace gfx::vk {
    struct Device;
    struct Pipeline;
    struct CommandBuffer;

    enum class CommandBufferType {
        none = 0,
        graphics,
        compute
    };

    struct CommandQueue {
        explicit CommandQueue(Device& device, CommandBufferType type, const std::wstring& name = L"Unnamed command queue");
        VkCommandBuffer& create_command_buffer(VkDevice& device, const Pipeline* pipeline, size_t frame_index);
        int clean_up_old_command_buffers(uint64_t curr_finished_index);
        void execute();

    private:
        VkCommandPool m_command_pool;
        CommandBufferType m_type = CommandBufferType::none;
        std::vector<VkCommandBuffer> m_command_buffer_pool;
        std::vector<size_t> m_command_lists_to_execute;
        std::deque<size_t> m_command_buffers_to_reuse;
    };
}
