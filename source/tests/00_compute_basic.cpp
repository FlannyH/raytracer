#include <memory>
#include "../device.h"
#include "../dx12/device.h"
#include "../vulkan/device.h"

constexpr size_t BYTE_COUNT = 512;

int main(int n_args, char** args) {
    uint32_t buffer_data[BYTE_COUNT];

    // Init
    auto device = std::make_unique<gfx::dx12::Device>(1280, 720, false, false); 
    auto buffer = device->create_buffer("buffer", sizeof(buffer_data), nullptr, gfx::ResourceUsage::cpu_read_write);
    auto pipeline = device->create_compute_pipeline("00_compute_basic", "assets/shaders/test/00_compute_basic.cs.hlsl");

    // Queue compute shader
    device->begin_compute_pass(pipeline, true);
    device->set_compute_root_constants({buffer.handle.as_u32()});
    device->dispatch_threadgroups(sizeof(buffer_data), 1, 1);
    device->end_compute_pass();

    // Run compute shader
    device->wait_async(device->execute_async());

    // Verify results
    device->readback_buffer(buffer, 0, sizeof(buffer_data), &buffer_data);
    for (size_t i = 0; i < BYTE_COUNT; ++i) {
        if (buffer_data[i] != i) {LOG(Error, "buffer_data[%i] == %i (expected %i)!", i, buffer_data[i], i);}
    }
}  
