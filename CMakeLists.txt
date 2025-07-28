cmake_minimum_required (VERSION 3.25)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif() 

# Update submodules as needed
find_package(Git QUIET)
if(GIT_FOUND)
  option(GIT_SUBMODULE "Check submodules during build" ON)
  if(GIT_SUBMODULE)
    message(STATUS "Submodule init")
    execute_process(COMMAND ${GIT_EXECUTABLE} submodule init
                    RESULT_VARIABLE GIT_SUBMOD_RESULT)
    if(NOT GIT_SUBMOD_RESULT EQUAL "0")
      message(FATAL_ERROR "git submodule init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
    endif()
    message(STATUS "Submodule update")
    execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    RESULT_VARIABLE GIT_SUBMOD_RESULT)
    if(NOT GIT_SUBMOD_RESULT EQUAL "0")
      message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
    endif()
  endif()
else()
  message(FATAL_ERROR "Failed to find Git!")
endif()

project ("raytracer")

# Add source to this project's executable.
add_executable (raytracer 
    # "source/tests/00_compute_basic.cpp"      
    "source/main.cpp"      
    "source/vulkan/descriptor_heap.cpp" "source/vulkan/descriptor_heap.h"    
    "source/vulkan/command_queue.cpp"   "source/vulkan/command_queue.h" 
    "source/vulkan/pipeline.cpp"        "source/vulkan/pipeline.h" 
    "source/vulkan/device.cpp"          "source/vulkan/device.h" 
    "source/vulkan/fence.cpp"           "source/vulkan/fence.h" 
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
    "external/mikktspace/mikktspace.c")
    
find_package(Vulkan REQUIRED COMPONENTS dxc)

target_link_libraries(raytracer glfw ${Vulkan_LIBRARIES} Vulkan::dxc_lib)

target_compile_definitions(raytracer PRIVATE _CRT_SECURE_NO_WARNINGS)
target_include_directories(raytracer PUBLIC ${Vulkan_INCLUDE_DIRS}
    "external/glfw/include"
    "external/glm"
    "external/stb"
    "external/tinygltf"
    "external/mikktspace"
)
add_subdirectory(external/glfw)
add_subdirectory(external/glm)
add_subdirectory(external/tinygltf)
if (WIN32)
    target_link_libraries(raytracer d3d12 dxgi)
endif()
set_property(TARGET raytracer PROPERTY CXX_STANDARD 20)

add_custom_target(copy_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/assets
)
add_dependencies(raytracer copy_assets)
