cmake_minimum_required(VERSION 3.20)
project(egg
    VERSION 0.1
    LANGUAGES CXX)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
include(FetchContent)
include(CompileShader)

find_package(Vulkan REQUIRED)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm
    GIT_PROGRESS TRUE)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
    flecs
    GIT_REPOSITORY https://github.com/SanderMertens/flecs
    GIT_TAG v3.2.6
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
    GIT_TAG master
    GIT_PROGRESS TRUE)
FetchContent_MakeAvailable(vma)

add_library(vmalib src/vma.cpp)
target_link_libraries(vmalib Vulkan::Vulkan)
set_target_properties(vmalib PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
target_compile_features(vmalib PRIVATE cxx_std_14)

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
target_compile_features(imguilib PRIVATE cxx_std_20)

include_directories(include/ ${imgui_SOURCE_DIR} ${vma_SOURCE_DIR}/include)

FetchContent_Declare(
    zstd
    GIT_REPOSITORY https://github.com/facebook/zstd
    GIT_TAG v1.5.5
    GIT_PROGRESS TRUE
    SOURCE_SUBDIR build/cmake)
set(ZSTD_MULTITHREAD_SUPPORT ON CACHE BOOL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zstd)
# TODO: this shouldn't be necessary??
include_directories(${zstd_SOURCE_DIR}/lib)

FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp
    GIT_TAG master
    GIT_PROGRESS TRUE)
# TODO: Assimp build currently fails with -Werror on GCC13
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(assimp)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb
    GIT_PROGRESS TRUE)
FetchContent_MakeAvailable(stb)
include_directories(include/ ${stb_SOURCE_DIR})
add_library(stblib src/stb.cpp)

#####
add_subdirectory(src/egg)
add_subdirectory(src/egg/renderer/algorithms/forward)
add_subdirectory(src/asset-bundler)
