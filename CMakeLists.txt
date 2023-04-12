cmake_minimum_required(VERSION 3.20)
include(FetchContent)

project(egg
    VERSION 0.1
    LANGUAGES CXX)

FetchContent_Declare(
    flecs
    GIT_REPOSITORY https://github.com/SanderMertens/flecs
    GIT_TAG v3.2.0)
FetchContent_MakeAvailable(flecs)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw
    GIT_TAG 3.3.8)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
    GIT_TAG v3.0.1)
FetchContent_MakeAvailable(vma)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG v1.89.4)
FetchContent_MakeAvailable(imgui)

add_library(imguilib
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
)
target_link_libraries(imguilib glfw Vulkan::Vulkan)

find_package(Vulkan REQUIRED)

include_directories(include/ ${imgui_SOURCE_DIR})
add_executable(egg
    src/egg/main.cpp
    src/egg/renderer/renderer.cpp
    src/egg/renderer/core/frame_renderer.cpp
    src/egg/renderer/core/device.cpp
    src/egg/renderer/core/swap_chain.cpp
    src/egg/renderer/imgui_renderer.cpp
    src/egg/renderer/scene_renderer.cpp)
target_compile_features(egg PUBLIC cxx_std_14)
target_link_libraries(egg flecs::flecs_static glfw Vulkan::Vulkan VulkanMemoryAllocator imguilib)

