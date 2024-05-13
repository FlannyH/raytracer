#pragma once
#include <wrl/client.h>
#include <exception>
#include <fstream>
#include <vector>
using Microsoft::WRL::ComPtr;

constexpr UINT backbuffer_count = 3;

inline void validate(const HRESULT hr) {
    if (FAILED(hr)) {
        throw std::exception();
    }
}

inline void read_file(const std::string& path, size_t& size_bytes, char*& data, const bool silent) {
    // Open file
    std::ifstream file_stream(path, std::ios::binary);

    // Is it actually open?
    if (file_stream.is_open() == false)
    {
        if (!silent)
            printf("[ERROR] Failed to open file '%s'!\n", path.c_str());
        size_bytes = 0;
        data = nullptr;
        return;
    }

    // See how big the file is so we can allocate the right amount of memory
    const auto begin = file_stream.tellg();
    file_stream.seekg(0, std::ifstream::end);
    const auto end = file_stream.tellg();
    const auto size = end - begin;
    size_bytes = static_cast<size_t>(size);

    // Allocate memory
    data = static_cast<char*>(malloc(static_cast<uint32_t>(size)));

    // Load file data into that memory
    file_stream.seekg(0, std::ifstream::beg);
    const std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(file_stream), {});

    // Is it actually open?
    if (buffer.empty())
    {
        if (!silent)
            printf("[ERROR] Failed to open file '%s'!\n", path.c_str());
        free(data);
        size_bytes = 0;
        data = nullptr;
        return;
    }
    memcpy(data, buffer.data(), size_bytes);
}
