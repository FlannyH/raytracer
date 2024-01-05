#pragma once
#include <wrl/client.h>
#include <exception>
using Microsoft::WRL::ComPtr;

constexpr UINT backbuffer_count = 3;

inline void validate(const HRESULT hr) {
    if (FAILED(hr)) {
        throw std::exception();
    }
}