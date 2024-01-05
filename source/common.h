#pragma once
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

inline void validate(const HRESULT hr) {
    if (FAILED(hr)) {
        throw std::exception();
    }
}