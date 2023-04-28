#include "egg/renderer/algorithms/forward.h"
#include "egg/renderer/core/frame_renderer.h"
#include "egg/renderer/memory.h"
#include <fstream>
#include <iostream>

const vk::Format DEPTH_BUFFER_FORMAT = vk::Format::eD24UnormS8Uint;

void forward_rendering_algorithm::init_with_device(vk::Device device, VmaAllocator allocator) {
    this->device    = device;
    this->allocator = allocator;
}

void forward_rendering_algorithm::create_static_objects(
    vk::AttachmentDescription present_surface_attachment
) {
    vk::AttachmentDescription attachments[]{
        present_surface_attachment,
        vk::AttachmentDescription{
                                  {},
                                  DEPTH_BUFFER_FORMAT, present_surface_attachment.samples,
                                  vk::AttachmentLoadOp::eClear,
                                  vk::AttachmentStoreOp::eDontCare,
                                  vk::AttachmentLoadOp::eDontCare,
                                  vk::AttachmentStoreOp::eDontCare,
                                  vk::ImageLayout::eUndefined,
                                  vk::ImageLayout::eDepthStencilAttachmentOptimal}
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
                               nullptr, &refs[1]}
    };

    // !!! Helpful:
    // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples-(Legacy-synchronization-APIs)
    vk::SubpassDependency depds[] {
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

    render_pass = device.createRenderPassUnique(vk::RenderPassCreateInfo{
        {}, 2, attachments, 1, subpasses, 2, depds});

    clear_values = {
        vk::ClearColorValue{0.f, 0.6f, 0.9f, 1.f},
        vk::ClearDepthStencilValue{1.f, 0}
    };

    render_pass_begin_info = vk::RenderPassBeginInfo{
        render_pass.get(), VK_NULL_HANDLE, {}, (uint32_t)clear_values.size(), clear_values.data()};

    depth_buffer_create_info = vk::ImageCreateInfo{
        {},
        vk::ImageType::e2D,
        DEPTH_BUFFER_FORMAT,
        {},
        1,
        1,
        present_surface_attachment.samples,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment,
        vk::SharingMode::eExclusive,
        {},
        {},
        vk::ImageLayout::eUndefined};

    cmd_buf_inherit_info = vk::CommandBufferInheritanceInfo{render_pass.get(), 0, VK_NULL_HANDLE};
}

vk::UniqueShaderModule load_shader(vk::Device device, const std::filesystem::path& bin_path) {
    std::ifstream file(bin_path, std::ios::ate | std::ios::binary);
    if(!file)
        throw std::runtime_error(std::string("failed to load shader at: ") + bin_path.c_str());

    std::vector<char> buffer((size_t)file.tellg());
    file.seekg(0);
    file.read(buffer.data(), buffer.size());
    file.close();

    return device.createShaderModuleUnique(vk::ShaderModuleCreateInfo{
        {}, buffer.size(), (uint32_t*)buffer.data()});
}

void forward_rendering_algorithm::create_pipeline_layouts(
    vk::DescriptorSetLayout scene_data_desc_set_layout,
    vk::PushConstantRange   per_object_push_constants_range
) {
    pipeline_layout = device.createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo{
        {}, 1, &scene_data_desc_set_layout, 1, &per_object_push_constants_range});
}

void forward_rendering_algorithm::create_pipelines() {
    // TODO: if we put these in the binary by exporting them from glslc as C files and compiling
    // them in, we don't have to bother with the path problem and we would only have a single
    // hot-reload target!
    vertex_shader   = load_shader(device, shader_path / "forward.vert.bin");
    fragment_shader = load_shader(device, shader_path / "forward.frag.bin");

    vk::PipelineShaderStageCreateInfo shader_stages[] = {
        {{}, vk::ShaderStageFlagBits::eVertex,   vertex_shader.get(),   "main"},
        {{}, vk::ShaderStageFlagBits::eFragment, fragment_shader.get(), "main"}
    };

    auto vertex_binding
        = vk::VertexInputBindingDescription{0, sizeof(vertex), vk::VertexInputRate::eVertex};
    auto vertex_input_info = vk::PipelineVertexInputStateCreateInfo{
        {}, 1, &vertex_binding, 4, vertex_attribute_description};

    auto input_assembly
        = vk::PipelineInputAssemblyStateCreateInfo{{}, vk::PrimitiveTopology::eTriangleList};

    auto viewport_state = vk::PipelineViewportStateCreateInfo{{}, 1, nullptr, 1, nullptr};

    auto rasterizer_state = vk::PipelineRasterizationStateCreateInfo{
        {},
        VK_FALSE,
        VK_FALSE,
        vk::PolygonMode::eFill,
        vk::CullModeFlagBits::eBack,
        vk::FrontFace::eCounterClockwise,
        VK_FALSE,
        0.f,
        0.f,
        0.f,
        1.f};

    auto multisample_state = vk::PipelineMultisampleStateCreateInfo{};

    auto depth_stencil_state = vk::PipelineDepthStencilStateCreateInfo{
        {}, VK_TRUE, VK_TRUE, vk::CompareOp::eLess, VK_FALSE, VK_FALSE};

    // TODO: so inelegant
    vk::PipelineColorBlendAttachmentState color_blend_att[] = {{}};
    for(auto& i : color_blend_att)
        i.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                           | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    auto color_blending_state = vk::PipelineColorBlendStateCreateInfo{
        {}, VK_FALSE, vk::LogicOp::eCopy, 1, color_blend_att};

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
            0}
    );
    if(res.result != vk::Result::eSuccess)
        throw std::runtime_error("failed to create pipeline: " + vk::to_string(res.result));
    pipeline = std::move(res.value);
}

void forward_rendering_algorithm::create_framebuffers(frame_renderer* fr) {
    depth_buffer_create_info.setExtent(vk::Extent3D(fr->extent(), 1));
    depth_buffer = std::make_unique<gpu_image>(
        allocator, depth_buffer_create_info, VmaAllocationCreateInfo{.usage = VMA_MEMORY_USAGE_AUTO}
    );

    depth_buffer_image_view = device.createImageViewUnique(vk::ImageViewCreateInfo{
        {},
        depth_buffer->get(),
        vk::ImageViewType::e2D,
        DEPTH_BUFFER_FORMAT,
        vk::ComponentMapping{},
        vk::ImageSubresourceRange{
         vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil, 0, 1, 0, 1}
    });

    framebuffers = fr->create_framebuffers(
        render_pass.get(),
        [&](size_t frame_index, std::vector<vk::ImageView>& views) {
            // TODO: we could probably create frame_count depth buffers, but ? idk will we ever
            // really render more than one frame at a time? seems unlikely
            views.emplace_back(depth_buffer_image_view.get());
        }
    );
    render_pass_begin_info.setRenderArea(vk::Rect2D(vk::Offset2D(), fr->extent()));
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
    std::function<void(vk::CommandBuffer, vk::PipelineLayout)> generate_draw_cmds
) {

    cb.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, pipeline_layout.get(), 0, scene_data_desc_set, {}
    );
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());
    generate_draw_cmds(cb, pipeline_layout.get());
}
