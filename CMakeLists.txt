cmake_minimum_required(VERSION 3.20)
include(FetchContent)

project(egg
    VERSION 0.1
    LANGUAGES CXX)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm
    GIT_PROGRESS TRUE)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
    flecs
    GIT_REPOSITORY https://github.com/SanderMertens/flecs
    GIT_TAG v3.2.0
    GIT_PROGRESS TRUE)
FetchContent_MakeAvailable(flecs)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw
    GIT_TAG 3.3.8
    GIT_PROGRESS TRUE)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
    GIT_TAG v3.0.1
    GIT_PROGRESS TRUE)
FetchContent_MakeAvailable(vma)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG v1.89.4
    GIT_PROGRESS TRUE)
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
include_directories(include/ ${imgui_SOURCE_DIR})

find_package(Vulkan REQUIRED)

FetchContent_Declare(
    zstd
    GIT_REPOSITORY https://github.com/facebook/zstd
    GIT_TAG v1.5.5
    GIT_PROGRESS TRUE
    SOURCE_SUBDIR build/cmake)
FetchContent_MakeAvailable(zstd)

FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp
    GIT_TAG v5.2.5
    GIT_PROGRESS TRUE)
FetchContent_MakeAvailable(assimp)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb
    GIT_PROGRESS TRUE)
FetchContent_MakeAvailable(stb)
include_directories(include/ ${stb_SOURCE_DIR})
add_library(stblib
    src/stb.cpp)

#####

add_executable(egg
    src/egg/main.cpp
    src/egg/renderer/renderer.cpp
    src/egg/renderer/core/frame_renderer.cpp
    src/egg/renderer/core/device.cpp
    src/egg/renderer/core/swap_chain.cpp
    src/egg/renderer/imgui_renderer.cpp
    src/egg/renderer/scene_renderer.cpp
    src/egg/renderer/memory.cpp
    src/egg/components.cpp
    src/egg/bundle.cpp)
target_compile_features(egg PUBLIC cxx_std_17)
target_link_libraries(egg
    flecs::flecs_static
    glm
    glfw
    Vulkan::Vulkan VulkanMemoryAllocator
    imguilib
    libzstd_static)

add_executable(asset-bundler src/asset-bundler/main.cpp src/asset-bundler/output_bundle.cpp src/asset-bundler/importer.cpp)
target_compile_features(asset-bundler PUBLIC cxx_std_17)
target_link_libraries(asset-bundler libzstd_static assimp stblib Vulkan::Vulkan)

