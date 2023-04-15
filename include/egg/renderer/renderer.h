#pragma once
#include <filesystem>
#define GLFW_INCLUDE_VULKAN
#include "egg/bundle.h"
#include <GLFW/glfw3.h>
#include <flecs.h>
#include <memory>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

/// an actual rendering algorithm ie shaders, pipelines and building command
/// buffers
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
    initializing Vulkan
    low level rendering: swap chain, render passes
    imgui init & render
    high level rendering: upload scene onto GPU
    render pipeline
*/
class renderer {
    vk::UniqueInstance         instance;
    vk::DebugReportCallbackEXT debug_report_callback;

    vk::UniqueSurfaceKHR window_surface;
    vk::SurfaceFormatKHR surface_format;
    uint32_t             surface_image_count;

    vk::UniqueDevice   dev;
    vk::PhysicalDevice phy_dev;
    uint32_t           graphics_queue_family_index, present_queue_family_index;
    vk::Queue          graphics_queue, present_queue;
    VmaAllocator       allocator;
    void               init_device(vk::Instance instance);

    frame_renderer* fr;
    imgui_renderer* ir;
    scene_renderer* sr;

  public:
    renderer(GLFWwindow* window, flecs::world& world, std::unique_ptr<render_pipeline> pipeline);

    void resize(GLFWwindow* window);

    std::shared_ptr<asset_bundle> load_bundle_direct_to_gpu(const std::filesystem::path& p);

    void render_frame();

    ~renderer();

    friend class frame_renderer;
    friend class imgui_renderer;
    friend class scene_renderer;
};
