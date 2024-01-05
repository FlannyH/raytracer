#pragma once
#include <wrl/client.h>
#include <exception>
using Microsoft::WRL::ComPtr;

inline void validate(const HRESULT hr) {
    if (FAILED(hr)) {
        throw std::exception();
    }
}