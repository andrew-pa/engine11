#include "egg/renderer/core/frame_renderer.h"
#include "error.h"
#include <iostream>

frame_renderer::frame_renderer(renderer* r, vk::Extent2D swapchain_extent)
    : r(r), swapchain_extent(swapchain_extent) {

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

void frame_renderer::init_swapchain() {
    // std::cout << "swapchain extent: " << swapchain_extent.width << "x" << swapchain_extent.height
    // << "\n";

    vk::SwapchainCreateInfoKHR cfo{
        {},
        r->window_surface.get(),
        r->surface_image_count,
        r->surface_format.format,
        r->surface_format.colorSpace,
        swapchain_extent,
        1,
        vk::ImageUsageFlagBits::eColorAttachment};

    if(r->graphics_queue_family_index != r->present_queue_family_index) {
        cfo.imageSharingMode      = vk::SharingMode::eConcurrent;
        cfo.queueFamilyIndexCount = 2;
        uint32_t q[]              = {r->graphics_queue_family_index, r->present_queue_family_index};
        cfo.pQueueFamilyIndices   = q;
    } else {
        cfo.imageSharingMode = vk::SharingMode::eExclusive;
    }

    auto surface_caps  = r->phy_dev.getSurfaceCapabilitiesKHR(r->window_surface.get());
    cfo.preTransform   = surface_caps.currentTransform;
    std::unordered_set<vk::CompositeAlphaFlagBitsKHR> supported_alpha{
        vk::CompositeAlphaFlagBitsKHR::eInherit,
        vk::CompositeAlphaFlagBitsKHR::eOpaque
    };
    for (auto i = supported_alpha.begin(); i != supported_alpha.end();) {
		if((*i & surface_caps.supportedCompositeAlpha) != surface_caps.supportedCompositeAlpha) {
            i = supported_alpha.erase(i);
        } else {
            i++;
        }
    }
    cfo.compositeAlpha = *supported_alpha.begin();
    cfo.presentMode    = vk::PresentModeKHR::eFifo;
    cfo.clipped        = VK_TRUE;

    swapchain        = r->dev->createSwapchainKHRUnique(cfo);
    swapchain_images = r->dev->getSwapchainImagesKHR(swapchain.get());

    vk::ImageViewCreateInfo ivcfo{
        {},
        nullptr,
        vk::ImageViewType::e2D,
        r->surface_format.format,
        {},
        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
    };

    swapchain_image_views.clear();
    for(auto img : swapchain_images) {
        ivcfo.image = img;
        swapchain_image_views.emplace_back(r->dev->createImageViewUnique(ivcfo));
    }

    if(swapchain_images.size() != command_buffers.size()) {
        command_buffers.clear();
        command_buffers = r->dev->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{
            r->command_pool.get(),
            vk::CommandBufferLevel::ePrimary,
            (uint32_t)swapchain_images.size()});
        command_buffer_ready_fences.clear();
        // command buffers always start ready
        vk::FenceCreateInfo fence_cfo{vk::FenceCreateFlagBits::eSignaled};
        for(size_t i = 0; i < command_buffers.size(); ++i)
            command_buffer_ready_fences.emplace_back(r->dev->createFenceUnique(fence_cfo));
    }
}

std::vector<vk::UniqueFramebuffer> frame_renderer::create_framebuffers(
    vk::RenderPass                                                  render_pass,
    const std::function<void(size_t, std::vector<vk::ImageView>&)>& custom_image_views
) {
    std::vector<vk::UniqueFramebuffer> framebuffers;
    framebuffers.reserve(swapchain_image_views.size());
    vk::FramebufferCreateInfo cfo{
        vk::FramebufferCreateFlags(),
        render_pass,
        0,
        nullptr,
        swapchain_extent.width,
        swapchain_extent.height,
        1};
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
    auto     err         = r->dev->acquireNextImageKHR(
        swapchain.get(), UINT64_MAX, image_available.get(), VK_NULL_HANDLE, &frame_index
    );
    if(err != vk::Result::eSuccess) {
        if(err == vk::Result::eSuboptimalKHR || err == vk::Result::eErrorOutOfDateKHR)
            reset_swapchain(swapchain_extent);
        else
            throw vulkan_runtime_error("failed to acquire next image", err);
    }
    frame f{.frame_index = frame_index, .frame_cmd_buf = command_buffers[frame_index].get()};
    // wait for command buffer to become ready
    err = r->dev->waitForFences(
        command_buffer_ready_fences[frame_index].get(), VK_TRUE, UINT64_MAX
    );
    if(err != vk::Result::eSuccess)
        throw vulkan_runtime_error("command buffer failed to become ready", err);
    r->dev->resetFences(command_buffer_ready_fences[frame_index].get());
    f.frame_cmd_buf.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit}
    );
    return f;
}

void frame_renderer::end_frame(frame&& frame) {
    frame.frame_cmd_buf.end();
    vk::PipelineStageFlags wait_stages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    r->graphics_queue.submit(
        vk::SubmitInfo{
            1,
            &image_available.get(),
            wait_stages,
            1,
            &command_buffers[frame.frame_index].get(),
            1,
            &render_finished.get()},
        command_buffer_ready_fences[frame.frame_index].get()
    );
    auto err = r->present_queue.presentKHR(vk::PresentInfoKHR{
        1, &render_finished.get(), 1, &swapchain.get(), &frame.frame_index});
    if(err != vk::Result::eSuccess) throw vulkan_runtime_error("failed to present frame", err);
}
