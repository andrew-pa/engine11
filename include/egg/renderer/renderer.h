#pragma once
#include <filesystem>
#include <functional>
#include <unordered_set>
#define GLFW_INCLUDE_VULKAN
#include "egg/bundle.h"
#include <GLFW/glfw3.h>
#include <flecs.h>
#include <memory>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include "egg/renderer/renderer_shared.h"

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

    vk::UniqueCommandPool command_pool;

    vk::UniqueCommandBuffer upload_cmds;
    vk::UniqueFence         upload_fence;

    frame_renderer* fr;
    imgui_renderer* ir;
    scene_renderer* sr;

    class shared_library_reloader* rendering_algo_lib_loader;

  public:
    renderer(
        GLFWwindow*                          window,
        std::shared_ptr<flecs::world>        world,
        const std::filesystem::path& rendering_algorithm_library_path
    );

    void start_resource_upload(const std::shared_ptr<asset_bundle>& assets);
    void wait_for_resource_upload_to_finish();

    void resize(GLFWwindow* window);

    void render_frame();

    inline vk::Device device() const { return dev.get(); }
    inline VmaAllocator gpu_allocator() const { return allocator; }

    inline abstract_imgui_renderer* imgui() const { return (abstract_imgui_renderer*)ir; }

    renderer(renderer&) = delete;
    renderer& operator=(renderer&) = delete;

    ~renderer();

    friend class frame_renderer;
    friend class imgui_renderer;
    friend class scene_renderer;
};
