#pragma once
#include <GLFW/glfw3.h>
#include <flecs.h>
#include <memory>
#include <vulkan/vulkan.hpp>

/// an actual rendering algorithm ie shaders, pipelines and building command buffers
class render_pipeline {
public:
    virtual ~render_pipeline() = default;
};

/// creates & manages swap chains, framebuffers, per-frame command buffer
class frame_renderer;
/// initializes and renders ImGUI layer
class imgui_renderer;
/// synchronizes scene data to GPU and manages actual rendering of the scene via a render pipeline
class scene_renderer;

/*
 * responsibilities:
    low level rendering: swap chain, render passes
    imgui init & render
    high level rendering: upload scene onto GPU
    render pipeline
*/
class renderer {
    vk::UniqueInstance instance;
    vk::DebugReportCallbackEXT debug_report_callback;

    frame_renderer* fr;
    imgui_renderer* ir;
    scene_renderer* sr;
public:
    renderer(
            GLFWwindow* window,
            flecs::world& world,
            std::unique_ptr<render_pipeline> pipeline);

    void render_frame();

    ~renderer();
};


