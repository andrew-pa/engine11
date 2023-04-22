#pragma once
#include <filesystem>
#include <functional>
#define GLFW_INCLUDE_VULKAN
#include "egg/bundle.h"
#include <GLFW/glfw3.h>
#include <flecs.h>
#include <memory>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

/// creates & manages swap chains, framebuffers, per-frame command buffer
class frame_renderer;
/// initializes and renders ImGUI layer
class imgui_renderer;
/// synchronizes scene data to GPU and manages actual rendering of the scene via a render pipeline
class scene_renderer;

/// an actual rendering algorithm ie shaders, pipelines and building command
/// buffers
class rendering_algorithm {
  public:
    // create render pass, descriptor pools etc
    virtual void create_static_objects(
        vk::Device device, vk::AttachmentDescription present_surface_attachment
    )                                                                                 = 0;
    virtual vk::RenderPassBeginInfo* get_render_pass_begin_info(uint32_t frame_index) = 0;
    // TODO: start_resource_upload/resource_upload_cleanup?
    // create pipeline layout and any algorithm specific descriptor sets/set layouts
    virtual void create_pipeline_layouts(
        vk::Device              device,
        vk::DescriptorSetLayout scene_data_desc_set_layout,
        vk::PushConstantRange   per_object_push_constants_range
    ) = 0;
    // load shaders and create pipelines
    virtual void create_pipelines(vk::Device device) = 0;
    // create framebuffers
    virtual void create_framebuffers(frame_renderer* fr) = 0;
    // generate command buffers
    virtual void generate_commands(
        vk::CommandBuffer                                          cb,
        vk::DescriptorSet                                          scene_data_desc_set,
        std::function<void(vk::CommandBuffer, vk::PipelineLayout)> generate_draw_cmds
    )                              = 0;
    virtual ~rendering_algorithm() = default;
};

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

  public:
    renderer(
        GLFWwindow*                          window,
        std::shared_ptr<flecs::world>        world,
        std::unique_ptr<rendering_algorithm> pipeline
    );

    void start_resource_upload(const std::shared_ptr<asset_bundle>& assets);
    void wait_for_resource_upload_to_finish();

    void resize(GLFWwindow* window);

    void render_frame();

    ~renderer();

    friend class frame_renderer;
    friend class imgui_renderer;
    friend class scene_renderer;
};
