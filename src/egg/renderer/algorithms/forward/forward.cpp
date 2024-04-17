#include "egg/renderer/algorithms/forward.h"
#include "asset-bundler/format.h"
#include "egg/renderer/memory.h"
#include "error.h"
#include <fs-shim.h>
#include <fstream>
#include <iostream>
#include <vulkan/vulkan_format_traits.hpp>

#define arraysize(A) (sizeof(A) / sizeof(A[0]))

const uint32_t vertex_shader_bytecode[] = {
#include "forward.vert.num"
};
const vk::ShaderModuleCreateInfo vertex_shader_create_info{
    {}, sizeof(vertex_shader_bytecode), vertex_shader_bytecode
};

const uint32_t fragment_shader_bytecode[] = {
#include "forward.frag.num"
};
const vk::ShaderModuleCreateInfo fragment_shader_create_info{
    {}, sizeof(fragment_shader_bytecode), fragment_shader_bytecode
};

const uint32_t skybox_vertex_shader_bytecode[] = {
#include "skybox.vert.num"
};
const vk::ShaderModuleCreateInfo skybox_vertex_shader_create_info{
    {}, sizeof(skybox_vertex_shader_bytecode), skybox_vertex_shader_bytecode
};

const uint32_t skybox_fragment_shader_bytecode[] = {
#include "skybox.frag.num"
};
const vk::ShaderModuleCreateInfo skybox_fragment_shader_create_info{
    {}, sizeof(skybox_fragment_shader_bytecode), skybox_fragment_shader_bytecode
};

renderer_features forward_rendering_algorithm::required_features() const {
    return {.raytracing = false};
}

void forward_rendering_algorithm::init_with_device(
    vk::Device                            device,
    std::shared_ptr<gpu_allocator>        allocator,
    const std::unordered_set<vk::Format>& supported_depth_formats
) {
    this->device    = device;
    this->allocator = allocator;

    depth_buffer_create_info = vk::ImageCreateInfo{
        {},
        vk::ImageType::e2D,
        vk::Format::eUndefined,
        {},
        1,
        1,
        vk::SampleCountFlagBits::e1,  // TODO: surely we'll never enable MSAA?
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment,
        vk::SharingMode::eExclusive,
        {},
        {},
        vk::ImageLayout::eUndefined
    };

    for(auto fmt : supported_depth_formats) {
        // select any 24bit format or any 32bit format w/o stencil
        // TODO is this true? we don't actually need stencil, but 24bit formats can be faster,
        // apparently?
        if(vk::componentBits(fmt, 0) == 24
           || (vk::componentBits(fmt, 0) == 32 && vk::componentCount(fmt) == 1)) {
            depth_buffer_create_info.format = fmt;
            break;
        }
    }

    std::cout << "using depth buffer format: " << vk::to_string(depth_buffer_create_info.format)
              << "\n";
}

void forward_rendering_algorithm::create_static_objects(
    vk::AttachmentDescription present_surface_attachment
) {
    vk::AttachmentDescription attachments[]{
        present_surface_attachment,
        vk::AttachmentDescription{
                                  {},
                                  depth_buffer_create_info.format,
                                  depth_buffer_create_info.samples,
                                  vk::AttachmentLoadOp::eClear,
                                  vk::AttachmentStoreOp::eDontCare,
                                  vk::AttachmentLoadOp::eDontCare,
                                  vk::AttachmentStoreOp::eDontCare,
                                  vk::ImageLayout::eUndefined,
                                  vk::ImageLayout::eDepthStencilAttachmentOptimal
        }
    };

    vk::AttachmentReference refs[]{
        {0, vk::ImageLayout::eColorAttachmentOptimal       },
 // we could possibly specify that we're not going to use the stencil, might be faster
        {1, vk::ImageLayout::eDepthStencilAttachmentOptimal}
    };

    vk::SubpassDescription subpasses[]{
        vk::SubpassDescription{
                               vk::SubpassDescriptionFlags(),
                               vk::PipelineBindPoint::eGraphics,
                               0, nullptr,
                               1, refs,
                               nullptr, &refs[1]
        }
    };

    // !!! Helpful:
    // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples-(Legacy-synchronization-APIs)
    vk::SubpassDependency depds[]{
        {VK_SUBPASS_EXTERNAL,
         0, vk::PipelineStageFlagBits::eColorAttachmentOutput,
         vk::PipelineStageFlagBits::eColorAttachmentOutput,
         {},
         vk::AccessFlagBits::eColorAttachmentWrite             },
        {VK_SUBPASS_EXTERNAL,
         0, vk::PipelineStageFlagBits::eLateFragmentTests,
         vk::PipelineStageFlagBits::eEarlyFragmentTests,
         vk::AccessFlagBits::eDepthStencilAttachmentWrite,
         vk::AccessFlagBits::eDepthStencilAttachmentRead
             | vk::AccessFlagBits::eDepthStencilAttachmentWrite}
    };

    render_pass = device.createRenderPassUnique(
        vk::RenderPassCreateInfo{{}, 2, attachments, 1, subpasses, 2, depds}
    );

    clear_values = {
        vk::ClearColorValue{std::array<float, 4>{0.f, 0.0f, 0.0f, 1.f}},
        vk::ClearDepthStencilValue{1.f,              0                       }
    };

    render_pass_begin_info = vk::RenderPassBeginInfo{
        render_pass.get(), VK_NULL_HANDLE, {}, (uint32_t)clear_values.size(), clear_values.data()
    };

    cmd_buf_inherit_info = vk::CommandBufferInheritanceInfo{render_pass.get(), 0, VK_NULL_HANDLE};
}

void forward_rendering_algorithm::create_pipeline_layouts(
    vk::DescriptorSetLayout scene_data_desc_set_layout,
    vk::PushConstantRange   per_object_push_constants_range
) {
    pipeline_layout = device.createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo{
        {}, 1, &scene_data_desc_set_layout, 1, &per_object_push_constants_range
    });
}

void forward_rendering_algorithm::create_pipelines() {
    if(!vertex_shader) vertex_shader = device.createShaderModuleUnique(vertex_shader_create_info);
    if(!fragment_shader)
        fragment_shader = device.createShaderModuleUnique(fragment_shader_create_info);

    vk::PipelineShaderStageCreateInfo shader_stages[] = {
        {{}, vk::ShaderStageFlagBits::eVertex,   vertex_shader.get(),   "main"},
        {{}, vk::ShaderStageFlagBits::eFragment, fragment_shader.get(), "main"}
    };

    auto vertex_binding
        = vk::VertexInputBindingDescription{0, sizeof(vertex), vk::VertexInputRate::eVertex};
    auto vertex_input_info = vk::PipelineVertexInputStateCreateInfo{
        {}, 1, &vertex_binding, 4, vertex_attribute_description
    };

    auto input_assembly
        = vk::PipelineInputAssemblyStateCreateInfo{{}, vk::PrimitiveTopology::eTriangleList};

    auto viewport_state = vk::PipelineViewportStateCreateInfo{{}, 1, nullptr, 1, nullptr};

    auto rasterizer_state = vk::PipelineRasterizationStateCreateInfo{
        {},
        VK_FALSE,
        VK_FALSE,
        vk::PolygonMode::eFill,
        vk::CullModeFlagBits::eBack,
        vk::FrontFace::eClockwise,
        VK_FALSE,
        0.f,
        0.f,
        0.f,
        1.f
    };

    auto multisample_state = vk::PipelineMultisampleStateCreateInfo{};

    auto depth_stencil_state = vk::PipelineDepthStencilStateCreateInfo{
        {}, VK_TRUE, VK_TRUE, vk::CompareOp::eLess, VK_FALSE, VK_FALSE
    };

    // TODO: so inelegant
    vk::PipelineColorBlendAttachmentState color_blend_att[] = {{}};
    for(auto& i : color_blend_att) {
        i.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                           | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    }
    auto color_blending_state = vk::PipelineColorBlendStateCreateInfo{
        {}, VK_FALSE, vk::LogicOp::eCopy, 1, color_blend_att
    };

    vk::DynamicState dynamic_states[]{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    auto             dynamic_state = vk::PipelineDynamicStateCreateInfo{{}, 2, dynamic_states};

    auto res = device.createGraphicsPipelineUnique(
        VK_NULL_HANDLE,
        vk::GraphicsPipelineCreateInfo{
            {},
            2,
            shader_stages,
            &vertex_input_info,
            &input_assembly,
            nullptr,
            &viewport_state,
            &rasterizer_state,
            &multisample_state,
            &depth_stencil_state,
            &color_blending_state,
            &dynamic_state,
            pipeline_layout.get(),
            render_pass.get(),
            0
        }
    );
    if(res.result != vk::Result::eSuccess)
        throw vulkan_runtime_error("failed to create pipeline", res.result);
    pipeline = std::move(res.value);

    if(!sky_vertex_shader)
        sky_vertex_shader = device.createShaderModuleUnique(skybox_vertex_shader_create_info);
    if(!sky_fragment_shader)
        sky_fragment_shader = device.createShaderModuleUnique(skybox_fragment_shader_create_info);

    vk::PipelineShaderStageCreateInfo sky_shader_stages[] = {
        {{}, vk::ShaderStageFlagBits::eVertex,   sky_vertex_shader.get(),   "main"},
        {{}, vk::ShaderStageFlagBits::eFragment, sky_fragment_shader.get(), "main"}
    };

    auto sky_vertex_binding
        = vk::VertexInputBindingDescription{0, sizeof(vec3), vk::VertexInputRate::eVertex};
    const vk::VertexInputAttributeDescription sky_vertex_attrib_desc[] = {
        {0, 0, vk::Format::eR32G32B32Sfloat, 0},
    };
    auto sky_vertex_input_info = vk::PipelineVertexInputStateCreateInfo{
        {}, 1, &sky_vertex_binding, 1, sky_vertex_attrib_desc
    };

    depth_stencil_state.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
    rasterizer_state.setCullMode(vk::CullModeFlagBits::eFront);

    res = device.createGraphicsPipelineUnique(
        VK_NULL_HANDLE,
        vk::GraphicsPipelineCreateInfo{
            {},
            2,
            sky_shader_stages,
            &sky_vertex_input_info,
            &input_assembly,
            nullptr,
            &viewport_state,
            &rasterizer_state,
            &multisample_state,
            &depth_stencil_state,
            &color_blending_state,
            &dynamic_state,
            pipeline_layout.get(),
            render_pass.get(),
            0
        }
    );
    if(res.result != vk::Result::eSuccess)
        throw vulkan_runtime_error("failed to create pipeline", res.result);
    sky_pipeline = std::move(res.value);
}

void forward_rendering_algorithm::create_framebuffers(abstract_frame_renderer* fr) {
    depth_buffer_create_info.setExtent(vk::Extent3D(fr->get_current_extent(), 1));
    depth_buffer = std::make_unique<gpu_image>(
        allocator, depth_buffer_create_info, VmaAllocationCreateInfo{.usage = VMA_MEMORY_USAGE_AUTO}
    );

    depth_buffer_image_view = device.createImageViewUnique(vk::ImageViewCreateInfo{
        {},
        depth_buffer->get(),
        vk::ImageViewType::e2D,
        depth_buffer_create_info.format,
        vk::ComponentMapping{},
        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1}
    });

    framebuffers = fr->create_framebuffers(
        render_pass.get(),
        [&](size_t frame_index, std::vector<vk::ImageView>& views) {
            // TODO: we could probably create frame_count depth buffers, but ? idk will we ever
            // really render more than one frame at a time? seems unlikely
            views.emplace_back(depth_buffer_image_view.get());
        }
    );
    render_pass_begin_info.setRenderArea(vk::Rect2D(vk::Offset2D(), fr->get_current_extent()));
}

vk::RenderPassBeginInfo* forward_rendering_algorithm::get_render_pass_begin_info(
    uint32_t frame_index
) {
    render_pass_begin_info.setFramebuffer(framebuffers[frame_index].get());
    return &this->render_pass_begin_info;
}

void forward_rendering_algorithm::generate_commands(
    vk::CommandBuffer                                          cb,
    vk::DescriptorSet                                          scene_data_desc_set,
    std::function<void(vk::CommandBuffer, vk::PipelineLayout)> generate_draw_cmds,
    std::function<void(vk::CommandBuffer, vk::PipelineLayout)> generate_skybox_cmds
) {

    cb.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, pipeline_layout.get(), 0, scene_data_desc_set, {}
    );
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());
    generate_draw_cmds(cb, pipeline_layout.get());

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, sky_pipeline.get());
    generate_skybox_cmds(cb, pipeline_layout.get());
}
