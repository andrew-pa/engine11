#include "egg/renderer/core/frame_renderer.h"
#include <iostream>

void frame_renderer::init_swapchain() {
    // std::cout << "swapchain extent: " << swapchain_extent.width << "x" << swapchain_extent.height << "\n";

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
    cfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eInherit;
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
            command_pool.get(), vk::CommandBufferLevel::ePrimary, (uint32_t)swapchain_images.size()});
    }
}
