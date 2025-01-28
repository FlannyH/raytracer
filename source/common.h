#pragma once
#include <wrl/client.h>
#include <exception>
#include <fstream>
#include <vector>
#include <cstdint>
#include "log.h"
using Microsoft::WRL::ComPtr;

constexpr UINT backbuffer_count = 3;

#define validate(hr) \
    if (FAILED(hr)) { \
        LOG(Error, "%s(%i): HRESULT 0x%08X", __FILE__, __LINE__, hr); \
        throw std::exception(); \
    }

#define to_fixed_16_16(n) (uint32_t)((n) * 65536)

inline void read_file(const std::string& path, size_t& size_bytes, char*& data, const bool silent) {
    // Open file
    std::ifstream file_stream(path, std::ios::binary);

    // Is it actually open?
    if (file_stream.is_open() == false)
    {
        if (!silent)
            LOG(Error, "Failed to open file '%s'!", path.c_str());
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
            LOG(Error, "Failed to open file '%s'!", path.c_str());
        free(data);
        size_bytes = 0;
        data = nullptr;
        return;
    }
    memcpy(data, buffer.data(), size_bytes);
}

template<typename T>
void add_and_align(T& destination, const T value_to_add, const T alignment) {
    destination += value_to_add;
    destination += (alignment - 1);
    destination -= (destination % alignment);
}

#define TODO() { LOG(Info, "%s(%i): TODO", __FILE__, __LINE__); exit(1); }
