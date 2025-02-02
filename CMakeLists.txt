# CMakeList.txt : CMake project for raytracer, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.15)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif() 

project ("raytracer")

# Add source to this project's executable.
add_executable (raytracer 
    "source/main.cpp"      
    "source/vulkan/descriptor_heap.cpp" "source/vulkan/descriptor_heap.h"    
    "source/vulkan/command_queue.cpp"   "source/vulkan/command_queue.h" 
    "source/vulkan/device.cpp"          "source/vulkan/device.h" 
    "source/dx12/descriptor_heap.cpp"   "source/dx12/descriptor_heap.h"    
    "source/dx12/command_buffer.cpp"    "source/dx12/command_buffer.h" 
    "source/dx12/command_queue.cpp"     "source/dx12/command_queue.h" 
    "source/dx12/swapchain.cpp"         "source/dx12/swapchain.h" 
    "source/dx12/pipeline.cpp"          "source/dx12/pipeline.h" 
    "source/dx12/device.cpp"            "source/dx12/device.h" 
    "source/dx12/fence.cpp"             "source/dx12/fence.h" 
    "source/shader.cpp"                 "source/shader.h"  
    "source/scene.cpp"                  "source/scene.h" 
    "source/input.cpp"                  "source/input.h"
    "source/tangent.cpp"                "source/tangent.h"
    "source/renderer.cpp"               "source/renderer.h"
    "source/log.cpp"                    "source/log.h"
    "external/include/mikktspace/mikktspace.c")
    
find_package(Vulkan REQUIRED FATAL_ERROR)

target_compile_definitions(raytracer PRIVATE _CRT_SECURE_NO_WARNINGS)
target_include_directories(raytracer PUBLIC "external/include" ${Vulkan_INCLUDE_DIRS})
target_link_directories(raytracer PUBLIC "external/libraries")
target_link_libraries(raytracer PUBLIC "glfw3.lib" "dxgi.lib" "D3d12.lib" "D3DCompiler.lib" "dxcompiler.lib" ${Vulkan_LIBRARIES})
set_property(TARGET raytracer PROPERTY CXX_STANDARD 20)

add_custom_target(copy_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/assets
)
add_custom_target(copy_dll
COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/external/dll ${CMAKE_CURRENT_BINARY_DIR}
)
add_dependencies(raytracer copy_assets)
add_dependencies(raytracer copy_dll)
