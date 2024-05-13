#include "command_queue.h"

#include "device.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include "common.h"
#include "pipeline.h"
#include "command_buffer.h"

namespace gfx {
    CommandQueue::CommandQueue(const Device& device, CommandBufferType type) {
        // Create command queue
        m_type = type;
        D3D12_COMMAND_QUEUE_DESC desc = {
            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        };

        switch (type) {
        case CommandBufferType::graphics:
            desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            break;
        case CommandBufferType::compute:
            desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
            break;
        }

        validate(device.device->CreateCommandQueue(&desc, IID_PPV_ARGS(&command_queue)));
    }

    std::shared_ptr<CommandBuffer> CommandQueue::create_command_buffer(const Device& device, const Pipeline* pipeline, uint64_t frame_index) {
        // Reuse if there's one available
        if (m_command_buffers_to_reuse.empty() == false) {
            size_t index_to_reuse = m_command_buffers_to_reuse.front();
            m_command_buffers_to_reuse.pop_front();
            m_in_flight_command_buffers.push_back(index_to_reuse);
            auto& cmd = m_command_buffer_pool[index_to_reuse];
            m_command_lists_to_execute.push_back(cmd);
            if (pipeline) 
                cmd->reset(pipeline->pipeline_state.Get(), frame_index);
            else
                cmd->reset(nullptr, frame_index);
            return cmd;
        }

        // Otherwise allocate a new one
        ID3D12PipelineState* pipeline_state = nullptr;
        if (pipeline != nullptr) pipeline_state = pipeline->pipeline_state.Get();
        auto cmd = std::make_shared<CommandBuffer>(device, pipeline_state, m_type, frame_index);
         
        m_in_flight_command_buffers.push_back(m_command_buffer_pool.size());
        m_command_buffer_pool.push_back(cmd);
        m_command_lists_to_execute.push_back(cmd);
        return cmd;
    }

    std::shared_ptr<CommandBuffer> CommandQueue::get_last_command_buffer() {
        return m_command_lists_to_execute.back();
    }

    void CommandQueue::execute() {
        if (m_command_lists_to_execute.empty()) return;

        std::vector<ID3D12CommandList*> cmds(m_command_lists_to_execute.size());
        for (size_t i = 0; i < cmds.size(); ++i) {
            auto cmd = m_command_lists_to_execute[i]->get();
            cmd->Close();
            cmds[i] = cmd;
        }
        
        command_queue->ExecuteCommandLists(cmds.size(), cmds.data());
        m_command_lists_to_execute.clear();
    }

    int CommandQueue::clean_up_old_command_buffers(const uint64_t curr_finished_index) {
        int n_cleaned_up = 0;
        while (m_in_flight_command_buffers.empty() == false) {
            auto id = m_in_flight_command_buffers.front();
            if (m_command_buffer_pool[id]->is_finished(curr_finished_index)) {
                // Add it to the reusable list
                m_command_buffers_to_reuse.push_back(id);
                m_in_flight_command_buffers.pop_front();
                ++n_cleaned_up;
                continue;
            } 
            break;
        }
        return n_cleaned_up;
    }
}
