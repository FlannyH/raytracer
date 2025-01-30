#include "command_queue.h"
#include "device.h"

namespace gfx {
    CommandQueue::CommandQueue(DeviceVulkan& device, CommandBufferType type, const std::wstring &name) {
        // todo: set command pool debug label?
        // todo: would VK_COMMAND_POOL_CREATE_TRANSIENT_BIT make sense here?
        VkCommandPoolCreateInfo pool_create_info { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_create_info.queueFamilyIndex = device.queue_family_indices().graphics_family.value();

        vkCreateCommandPool(device.device, &pool_create_info, nullptr, &m_command_pool);
    }

    VkCommandBuffer& CommandQueue::create_command_buffer(VkDevice& device, const Pipeline* pipeline, size_t frame_index) {        
        // Reuse if there's one available
        if (m_command_buffers_to_reuse.empty() == false) {
            size_t index_to_reuse = m_command_buffers_to_reuse.front();
            m_command_buffers_to_reuse.pop_front();
            m_command_lists_to_execute.push_back(index_to_reuse);
            auto& cmd = m_command_buffer_pool[index_to_reuse];
            vkResetCommandBuffer(cmd, 0);
            return cmd;
        }

        // todo: destroy cmd buffers on shutdown
        VkCommandBufferAllocateInfo alloc_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        alloc_info.commandPool = m_command_pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;

        const auto cmd_index = m_command_buffer_pool.size();
        m_command_lists_to_execute.push_back(cmd_index);
        m_command_buffer_pool.push_back({});
        auto& cmd = m_command_buffer_pool[cmd_index];
        vkAllocateCommandBuffers(device, &alloc_info, &cmd);
         
        return cmd;
    }
}
