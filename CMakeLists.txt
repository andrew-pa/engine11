cmake_minimum_required(VERSION 3.11)
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

include_directories(include/)
add_executable(egg
    src/main.cpp
    src/renderer/renderer.cpp
    src/renderer/frame_renderer.cpp
    src/renderer/imgui_renderer.cpp
    src/renderer/scene_renderer.cpp)
target_compile_features(egg PUBLIC cxx_std_14)
target_link_libraries(egg flecs::flecs_static glfw)

