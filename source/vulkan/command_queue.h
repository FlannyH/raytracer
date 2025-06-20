#pragma once
#include <vulkan/vulkan.h>
#include <deque>
#include <memory>

#include "../common.h"
#include "fence.h"

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
        void add_fence_wait_value(const Fence& fence, size_t value);
        void add_fence_signal_value(const Fence& fence, size_t value);
        void execute();

    private:
        VkCommandPool m_command_pool;
        VkQueue queue;
        CommandBufferType m_type = CommandBufferType::none;
        std::vector<VkCommandBuffer> m_command_buffer_pool;
        std::vector<size_t> m_command_lists_to_execute;
        std::deque<size_t> m_command_buffers_to_reuse;
        std::vector<uint64_t> m_wait_values;
        std::vector<uint64_t> m_signal_values;
        std::vector<VkSemaphore> m_wait_sems;
        std::vector<VkSemaphore> m_signal_sems;
    };
}
