#include <cstdio>
#include "device.h"

int main(int n_args, char** args) {
    gfx::create_window(1280, 720);
    gfx::init_device(true);
    while (1);
}