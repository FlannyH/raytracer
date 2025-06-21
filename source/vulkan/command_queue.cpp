#include "command_queue.h"
#include "device.h"
#include "fence.h"

namespace gfx::vk {
    CommandQueue::CommandQueue(Device& device, CommandBufferType type, const std::wstring &name) {
        // todo: set command pool debug label?
        // todo: would VK_COMMAND_POOL_CREATE_TRANSIENT_BIT make sense here?
        VkCommandPoolCreateInfo pool_create_info { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_create_info.queueFamilyIndex = device.queue_family_indices().graphics_family.value();

        vkCreateCommandPool(device.device, &pool_create_info, nullptr, &m_command_pool);
        vkGetDeviceQueue(device.device, pool_create_info.queueFamilyIndex, 0, &this->queue);
    }

    // todo: do we even need the pipeline here?
    VkCommandBuffer& CommandQueue::create_command_buffer(VkDevice& device, const Pipeline* pipeline, size_t frame_index) {        
        // Reuse if there's one available
        if (m_command_buffers_to_reuse.empty() == false) {
            size_t index_to_reuse = m_command_buffers_to_reuse.front();
            m_command_buffers_to_reuse.pop_front();
            m_command_lists_to_execute.push_back(index_to_reuse);
            auto& cmd = m_command_buffer_pool[index_to_reuse];
            vkResetCommandBuffer(cmd, 0);

            VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            vkBeginCommandBuffer(cmd, &begin_info);

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

        VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        
        vkBeginCommandBuffer(cmd, &begin_info);
        
        return cmd;
    }
    
    void CommandQueue::add_fence_wait_value(const Fence& fence, size_t value) {
            this->m_wait_sems.push_back(fence.timeline_semaphore);
            this->m_wait_values.push_back(value);
    }
    
    void CommandQueue::add_fence_signal_value(const Fence& fence, size_t value) {
            this->m_signal_sems.push_back(fence.timeline_semaphore);
            this->m_signal_values.push_back(value);
    }

    void CommandQueue::execute() {
        if (m_command_lists_to_execute.empty()) return;

        const std::vector<VkPipelineStageFlags> stage_flags(this->m_wait_values.size(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

        std::vector<VkCommandBuffer> cmds;
        cmds.reserve(this->m_command_lists_to_execute.size());
        for (size_t i = 0; i < this->m_command_lists_to_execute.size(); ++i) {
            cmds.push_back(this->m_command_buffer_pool[this->m_command_lists_to_execute[i]]);
            vkEndCommandBuffer(cmds[i]);
        }

        const VkTimelineSemaphoreSubmitInfo sem_submit_info = {
            .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreValueCount = (uint32_t)this->m_wait_values.size(),
            .pWaitSemaphoreValues = this->m_wait_values.data(),
            .signalSemaphoreValueCount = (uint32_t)this->m_signal_values.size(),
            .pSignalSemaphoreValues = this->m_signal_values.data(),
        };

        const VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = &sem_submit_info,
            .waitSemaphoreCount = (uint32_t)this->m_wait_sems.size(),
            .pWaitSemaphores = this->m_wait_sems.data(),
            .pWaitDstStageMask = stage_flags.data(),
            .commandBufferCount = (uint32_t)this->m_command_lists_to_execute.size(),
            .pCommandBuffers = cmds.data(),
            .signalSemaphoreCount = (uint32_t)this->m_signal_sems.size(),
            .pSignalSemaphores = this->m_signal_sems.data(),
        };
        vkQueueSubmit(this->queue, 1, &submit_info, VK_NULL_HANDLE);

        this->m_command_lists_to_execute.clear();
        this->m_signal_values.clear();
        this->m_signal_sems.clear();
        this->m_wait_values.clear();
        this->m_wait_sems.clear();
    }
}
