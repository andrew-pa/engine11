#pragma once
#include "egg/renderer/memory.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <unordered_set>
#include <vulkan/vulkan.hpp>

#ifdef _MSC_VER
#    define SHARED_EXPORT __declspec(dllexport)
#else
#    define SHARED_EXPORT
#endif

/// creates & manages swap chains, framebuffers, per-frame command buffer
class frame_renderer;  // the actual implementation of frame_renderer_abstract

class abstract_frame_renderer {
    // provided so that rendering_algorithms can be in a shared library
  public:
    virtual vk::Extent2D                       get_current_extent() const = 0;
    virtual std::vector<vk::UniqueFramebuffer> create_framebuffers(
        vk::RenderPass                                                  render_pass,
        const std::function<void(size_t, std::vector<vk::ImageView>&)>& custom_image_views
        = [](auto, auto) {}
    )                                  = 0;
    virtual ~abstract_frame_renderer() = default;
};

/// initializes and renders ImGUI layer
class imgui_renderer;

class abstract_imgui_renderer {
  public:
    // TODO: have a clean way to remove windows so they don't persist after their parent object has
    // been destroyed
    virtual void add_window(const std::string& name, const std::function<void(bool*)>& draw) = 0;
    virtual uint64_t add_texture(vk::ImageView image_view, vk::ImageLayout image_layout)     = 0;
    virtual ~abstract_imgui_renderer() = default;
};

/// synchronizes scene data to GPU and manages actual rendering of the scene via a render pipeline
class scene_renderer;

struct renderer_features {
    bool raytracing = false;
};

/// an actual rendering algorithm ie shaders, pipelines and building command
/// buffers
class rendering_algorithm {
  public:
    virtual renderer_features required_features() const = 0;
    virtual void              init_with_device(
                     vk::Device                            device,
                     std::shared_ptr<gpu_allocator>        allocator,
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
    virtual void                              generate_commands(
                                     vk::CommandBuffer                                          cb,
                                     vk::DescriptorSet                                          scene_data_desc_set,
                                     std::function<void(vk::CommandBuffer, vk::PipelineLayout)> generate_draw_cmds,
                                     std::function<void(vk::CommandBuffer, vk::PipelineLayout)> generate_skybox_cmds
                                 ) = 0;
    virtual ~rendering_algorithm() = default;
};
