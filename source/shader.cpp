#include "shader.h"
#include <iostream>
#include <fstream>

namespace gfx {
    ComPtr<IDxcCompiler3> _dxc_compiler = nullptr;
    ComPtr<IDxcUtils> _dxc_utils = nullptr;
    ComPtr<IDxcIncludeHandler> _dxc_include_handler = nullptr;

    std::string profile_from_shader_type(const ShaderType type) {
        switch (type) {
        case ShaderType::vertex:
            return "vs_6_6";
        case ShaderType::pixel:
            return "ps_6_6";
        case ShaderType::compute:
            return "cs_6_6";
        default:
            return "";
        }
    }

    std::wstring to_wstring(const std::string& s) {
        std::wstring temp(s.size(), L' ');
        for (size_t i = 0; i < s.size(); ++i) {
            temp[i] = s[i];
        }
        return temp;
    }

    std::string to_string(const std::wstring& s) {
        std::string temp(s.size(), L' ');
        for (size_t i = 0; i < s.size(); ++i) {
            temp[i] = (char)s[i];
        }
        return temp;
    }

    Shader::Shader(const std::string& path, const std::string& entry_point, const ShaderType type) {
        // Init dxc
        if (_dxc_compiler.Get() == nullptr) {
            DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&_dxc_compiler));
            DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&_dxc_utils));
            _dxc_utils->CreateDefaultIncludeHandler(&_dxc_include_handler);
        }

        // Load shader file
        const auto wpath = to_wstring(path);
        const auto wentry_point = to_wstring(entry_point);
        const auto wtype = to_wstring(profile_from_shader_type(type));
        ComPtr<IDxcBlobEncoding> source_blob;
        _dxc_utils->LoadFile(to_wstring(path).c_str(), nullptr, &source_blob);

        if (source_blob.Get() == nullptr) {
            printf("[ERROR] Could not load file '%s'! Does the file exist?\n", path.c_str());
            return;
        }

        // Set up compilation arguments
        std::vector<LPCWSTR> args;

        // TODO: add more args for optimization, debug, stripping reflect/debug
        args.emplace_back(wpath.c_str());
        args.emplace_back(L"-T");
        args.emplace_back(wtype.c_str());
        args.emplace_back(L"-E");
        args.emplace_back(wentry_point.c_str());
        args.emplace_back(L"-Qstrip_debug");
        args.emplace_back(L"-Qstrip_reflect");
        args.emplace_back(DXC_ARG_WARNINGS_ARE_ERRORS); //-WX
        args.emplace_back(DXC_ARG_DEBUG); //-Zi

        // Compile it
        const DxcBuffer buffer {
            .Ptr = source_blob->GetBufferPointer(),
            .Size = source_blob->GetBufferSize(),
            .Encoding = DXC_CP_ACP,
        };

        ComPtr<IDxcResult> result = nullptr;
        _dxc_compiler->Compile(
            &buffer, 
            args.data(), 
            (UINT32)args.size(),
            _dxc_include_handler.Get(), 
            IID_PPV_ARGS(&result)
        );

        // Check for errors
        ComPtr<IDxcBlobUtf8> errors;
        validate(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr));
         
        if (errors != nullptr && errors->GetStringLength() > 0) {
            printf("[ERROR] Error compiling shader '%s':\n\t%s\n", path.c_str(), errors->GetStringPointer());
        }

        // Get PDB file
        ComPtr<IDxcBlob> pdb_data;
        ComPtr<IDxcBlobUtf16> pdb_path;
        validate(result->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pdb_data), &pdb_path));
        auto pdb_path_string = to_string((wchar_t*)pdb_path->GetStringPointer());
        FILE* pdb = fopen(pdb_path_string.c_str(), "wb"); 
        fwrite(pdb_data->GetBufferPointer(), 1, pdb_data->GetBufferSize(), pdb);
        fclose(pdb);

        // Get shader blob
        ComPtr<IDxcBlob> dxc_shader_blob = nullptr;
        validate(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&dxc_shader_blob), nullptr)); 

        shader_blob = reinterpret_cast<ID3DBlob*>(dxc_shader_blob.Detach());
    }
}
