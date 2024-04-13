#pragma once
#include "egg/renderer/memory.h"
#include "egg/renderer/renderer_shared.h"
#include <utility>

class forward_rendering_algorithm : public rendering_algorithm {
    vk::Device                     device;
    std::shared_ptr<gpu_allocator> allocator;

    vk::UniqueRenderPass             render_pass;
    std::vector<vk::ClearValue>      clear_values;
    vk::RenderPassBeginInfo          render_pass_begin_info;
    vk::CommandBufferInheritanceInfo cmd_buf_inherit_info;

    std::vector<vk::UniqueFramebuffer> framebuffers;

    vk::ImageCreateInfo        depth_buffer_create_info;
    std::unique_ptr<gpu_image> depth_buffer;
    vk::UniqueImageView        depth_buffer_image_view;

    vk::UniquePipelineLayout pipeline_layout;
    vk::UniquePipeline       pipeline;
    vk::UniqueShaderModule   vertex_shader, fragment_shader;

    vk::UniquePipeline     sky_pipeline;
    vk::UniqueShaderModule sky_vertex_shader, sky_fragment_shader;

  public:
    renderer_features required_features() const override;

    void init_with_device(
        vk::Device                            device,
        std::shared_ptr<gpu_allocator>        allocator,
        const std::unordered_set<vk::Format>& supported_depth_formats
    ) override;

    void create_static_objects(vk::AttachmentDescription present_surface_attachment) override;

    vk::RenderPass get_render_pass() override { return render_pass.get(); }

    vk::RenderPassBeginInfo* get_render_pass_begin_info(uint32_t frame_index) override;

    void create_pipeline_layouts(
        vk::DescriptorSetLayout scene_data_desc_set_layout,
        vk::PushConstantRange   per_object_push_constants_range
    ) override;

    void create_pipelines() override;

    void create_framebuffers(abstract_frame_renderer* fr) override;

    vk::CommandBufferInheritanceInfo* get_command_buffer_inheritance_info() override {
        return &cmd_buf_inherit_info;
    }

    void generate_commands(
        vk::CommandBuffer                                          cb,
        vk::DescriptorSet                                          scene_data_desc_set,
        std::function<void(vk::CommandBuffer, vk::PipelineLayout)> generate_draw_cmds,
        std::function<void(vk::CommandBuffer, vk::PipelineLayout)> generate_skybox_cmds
    ) override;

    ~forward_rendering_algorithm() override = default;
};
