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

/// creates & manages swap chains, framebuffers, per-frame command buffer
class frame_renderer; //the actual implementation of frame_renderer_abstract
class abstract_frame_renderer {
    // provided so that rendering_algorithms can be in a shared library
public:
    virtual vk::Extent2D get_current_extent() const = 0;
    virtual std::vector<vk::UniqueFramebuffer> create_framebuffers(
        vk::RenderPass                                                  render_pass,
        const std::function<void(size_t, std::vector<vk::ImageView>&)>& custom_image_views
        = [](auto, auto) {}
    ) = 0;
};

/// initializes and renders ImGUI layer
class imgui_renderer;
class abstract_imgui_renderer {
public:
    virtual void add_window(const std::string& name, const std::function<void(bool*)>& draw) = 0;
    virtual uint64_t add_texture(vk::ImageView image_view, vk::ImageLayout image_layout) = 0;
};

/// synchronizes scene data to GPU and manages actual rendering of the scene via a render pipeline
class scene_renderer;

/// an actual rendering algorithm ie shaders, pipelines and building command
/// buffers
class rendering_algorithm {
  public:
    virtual void init_with_device(
        vk::Device                            device,
        VmaAllocator                          allocator,
        const std::unordered_set<vk::Format>& supported_depth_formats
    ) = 0;
    // create render pass, descriptor pools etc
    virtual void create_static_objects(vk::AttachmentDescription present_surface_attachment) = 0;
    virtual vk::RenderPass           get_render_pass()                                       = 0;
    virtual vk::RenderPassBeginInfo* get_render_pass_begin_info(uint32_t frame_index)        = 0;
    // TODO: start_resource_upload/resource_upload_cleanup?
    // create pipeline layout and any algorithm specific descriptor sets/set layouts
    virtual void create_pipeline_layouts(
        vk::DescriptorSetLayout scene_data_desc_set_layout,
        vk::PushConstantRange   per_object_push_constants_range
    ) = 0;
    // load shaders and create pipelines
    virtual void create_pipelines() = 0;
    // create framebuffers
    virtual void create_framebuffers(abstract_frame_renderer* fr) = 0;
    // generate command buffers
    virtual vk::CommandBufferInheritanceInfo* get_command_buffer_inheritance_info() = 0;
    virtual void generate_commands(
                                     vk::CommandBuffer                                          cb,
                                     vk::DescriptorSet                                          scene_data_desc_set,
                                     std::function<void(vk::CommandBuffer, vk::PipelineLayout)> generate_draw_cmds
                                 ) = 0;
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

    inline vk::Device device() const { return dev.get(); }
    inline VmaAllocator gpu_allocator() const { return allocator; }

    inline abstract_imgui_renderer* imgui() const { return (abstract_imgui_renderer*)ir; }

    ~renderer();

    friend class frame_renderer;
    friend class imgui_renderer;
    friend class scene_renderer;
};
