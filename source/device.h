#pragma once
namespace gfx {
    void create_window(int width, int height);
    void resize_window(int width, int height);
    void get_window_size(int& width, int& height);
    void init_device(bool debug_layer_enabled);
};