#include "egg/renderer/core/frame_renderer.h"
#include <iostream>

frame_renderer::frame_renderer(renderer* r, vk::Extent2D swapchain_extent) : r(r), swapchain_extent(swapchain_extent) {
    command_pool = r->dev->createCommandPoolUnique(vk::CommandPoolCreateInfo{
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer, r->graphics_queue_family_index});

    init_swapchain();

    vk::SemaphoreCreateInfo spcfo;
    image_available = r->dev->createSemaphoreUnique(spcfo);
    render_finished = r->dev->createSemaphoreUnique(spcfo);
}

void frame_renderer::reset_swapchain(vk::Extent2D new_swapchain_extent) {
    swapchain.reset();
    r->dev->waitIdle();
    swapchain_extent = new_swapchain_extent;
    init_swapchain();
}

std::vector<vk::UniqueFramebuffer> frame_renderer::create_framebuffers(
    vk::RenderPass render_pass, const std::function<void(size_t, std::vector<vk::ImageView>&)>& custom_image_views
) {
    std::vector<vk::UniqueFramebuffer> framebuffers;
    framebuffers.reserve(swapchain_image_views.size());
    vk::FramebufferCreateInfo cfo{
        vk::FramebufferCreateFlags(), render_pass, 0, nullptr, swapchain_extent.width, swapchain_extent.height, 1};
    std::vector<vk::ImageView> att;
    for(size_t i = 0; i < swapchain_image_views.size(); ++i) {
        att.clear();
        att.emplace_back(swapchain_image_views[i].get());
        custom_image_views(i, att);
        cfo.pAttachments    = att.data();
        cfo.attachmentCount = att.size();
        framebuffers.emplace_back(r->dev->createFramebufferUnique(cfo));
    }
    return framebuffers;
}

frame frame_renderer::begin_frame() {
    uint32_t frame_index = -1;
    auto err = r->dev->acquireNextImageKHR(swapchain.get(), UINT64_MAX, image_available.get(), VK_NULL_HANDLE, &frame_index);
    if(err != vk::Result::eSuccess) {
        if(err == vk::Result::eSuboptimalKHR || err == vk::Result::eErrorOutOfDateKHR)
            reset_swapchain(swapchain_extent);
        else
            throw std::runtime_error("failed to acquire next image: " + std::to_string((size_t)err));
    }
    frame f{.frame_index = frame_index, .frame_cmd_buf = command_buffers[frame_index].get()};
    f.frame_cmd_buf.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    return f;
}

void frame_renderer::end_frame(frame&& frame) {
    frame.frame_cmd_buf.end();
    vk::PipelineStageFlags wait_stages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    r->graphics_queue.submit(vk::SubmitInfo{
        1, &image_available.get(), wait_stages, 1, &command_buffers[frame.frame_index].get(), 1, &render_finished.get()});
    r->present_queue.presentKHR(vk::PresentInfoKHR{1, &render_finished.get(), 1, &swapchain.get(), &frame.frame_index});
}
