#pragma once
void validate(const HRESULT hr) {
    if (FAILED(hr)) {
        throw std::exception();
    }
}