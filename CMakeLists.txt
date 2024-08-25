# CMakeList.txt : CMake project for raytracer, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif() 

project ("raytracer")

# Add source to this project's executable.
add_executable (raytracer 
    "source/main.cpp"      
    "source/device.cpp"             "source/device.h" 
    "source/command_queue.cpp"      "source/command_queue.h" 
    "source/swapchain.cpp"          "source/swapchain.h" 
    "source/descriptor_heap.cpp"    "source/descriptor_heap.h"    
    "source/fence.cpp"              "source/fence.h" 
    "source/buffer.cpp"             "source/buffer.h"    
    "source/shader.cpp"             "source/shader.h"  
    "source/pipeline.cpp"           "source/pipeline.h" 
    "source/command_buffer.cpp"     "source/command_buffer.h" 
    "source/scene.cpp"              "source/scene.h" 
    "source/input.cpp"              "source/input.h"
    "source/tangent.cpp"            "source/tangent.h"
    "source/renderer.cpp"           "source/renderer.h"
    "external/include/mikktspace/mikktspace.c")
    
target_compile_definitions(raytracer PRIVATE _CRT_SECURE_NO_WARNINGS)
target_include_directories(raytracer PUBLIC "external/include")
target_link_directories(raytracer PUBLIC "external/libraries")
target_link_libraries(raytracer PUBLIC "glfw3.lib" "dxgi.lib" "D3d12.lib" "D3DCompiler.lib" "dxcompiler.lib")
set_property(TARGET raytracer PROPERTY CXX_STANDARD 20)

add_custom_command(
    TARGET raytracer POST_BUILD
    COMMAND robocopy /e /xx
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/
            ${CMAKE_CURRENT_BINARY_DIR}/assets/
            || (exit 0)
)
add_custom_command(
    TARGET raytracer POST_BUILD
    COMMAND robocopy /e /xx
            ${CMAKE_CURRENT_SOURCE_DIR}/external/dll/
            ${CMAKE_CURRENT_BINARY_DIR}/
            || (exit 0)
)
# TODO: Add tests and install targets if needed.
