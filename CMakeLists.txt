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
add_executable (raytracer "source/main.cpp" "source/device.h" "source/device.cpp"   "source/common.h" "source/command.h" "source/command.cpp" "source/swapchain.cpp" "source/swapchain.h" "source/descriptor_heap.h" "source/descriptor_heap.cpp" "source/hl_renderer.h" "source/hl_renderer.cpp"  "source/fence.h" "source/fence.cpp")

target_include_directories(raytracer PUBLIC "external/include")
target_link_directories(raytracer PUBLIC "external/libraries")
target_link_libraries(raytracer PUBLIC "glfw3.lib" "dxgi.lib" "D3d12.lib")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET raytracer PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
