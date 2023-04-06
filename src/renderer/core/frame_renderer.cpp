#include "egg/renderer/core/frame_renderer.h"
#include <iostream>

frame_renderer::frame_renderer(renderer* r) : r(r) {
    command_pool = r->dev->createCommandPoolUnique(vk::CommandPoolCreateInfo{
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer, r->graphics_queue_family_index});

    init_swapchain();
}

void frame_renderer::reset_swapchain() {
    swapchain.reset();
    r->dev->waitIdle();
    init_swapchain();
}

frame frame_renderer::begin_frame() {
    uint32_t frame_index = -1;
    auto err = r->dev->acquireNextImageKHR(swapchain.get(), UINT64_MAX, image_available.get(), VK_NULL_HANDLE, &frame_index);
    if(err != vk::Result::eSuccess) {
        if(err == vk::Result::eSuboptimalKHR || err == vk::Result::eErrorOutOfDateKHR)
            reset_swapchain();
        else
            throw std::runtime_error("failed to acquire next image: " + std::to_string((size_t)err));
    }
    return frame{.frame_index = frame_index, .frame_cmd_buf = command_buffers[frame_index].get()};
}

void frame_renderer::end_frame(frame&& frame) {
    vk::PipelineStageFlags wait_stages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    r->graphics_queue.submit(vk::SubmitInfo{
        1, &image_available.get(), wait_stages, 1, &frame.frame_cmd_buf, 1, &render_finished.get()});
    r->present_queue.presentKHR(vk::PresentInfoKHR{1, &render_finished.get(), 1, &swapchain.get(), &frame.frame_index});
}
