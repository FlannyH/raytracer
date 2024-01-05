#include "device.h"
#include "swapchain.h"
#include "command.h"
#include "descriptor_heap.h"

int main(int n_args, char** args) {
    gfx::create_window(1280, 720);
    gfx::init_device(true);
    gfx::create_command_queue();
    gfx::create_descriptor_heaps();
    gfx::create_swapchain();
    while (1);
}