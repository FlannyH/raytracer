#pragma once
#include <string>

#include "common.h"
#include "dxc/dxcapi.h"
#include <d3dcommon.h>

namespace gfx {
    struct Device;

    enum class ShaderType {
        vertex,
        pixel,
        compute,
    };

    std::string profile_from_shader_type(ShaderType type);

    struct Shader {
        explicit Shader(const std::string& path, const std::string& entry_point, ShaderType type);

    public:
        ComPtr<ID3DBlob> shader_blob;
    };
}
