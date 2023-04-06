#include "egg/renderer/core/frame_renderer.h"
#include <iostream>

void frame_renderer::init_swapchain() {
    auto     surface_caps = phy_dev.getSurfaceCapabilitiesKHR(window_surface.get());
    uint32_t image_count  = surface_caps.minImageCount + 1;
    std::cout << "using a swap chain with " << image_count << " images\n";

    swapchain_format = vk::Format::eB8G8R8A8Unorm;
    swapchain_extent = surface_caps.currentExtent;

    vk::SwapchainCreateInfoKHR cfo{
        {},
        window_surface.get(),
        image_count,
        swapchain_format,
        vk::ColorSpaceKHR::eSrgbNonlinear,
        swapchain_extent,
        1,
        vk::ImageUsageFlagBits::eColorAttachment};

    if(graphics_queue != present_queue) {
        cfo.imageSharingMode      = vk::SharingMode::eConcurrent;
        cfo.queueFamilyIndexCount = 2;
        throw 3;
    } else {
        cfo.imageSharingMode = vk::SharingMode::eExclusive;
    }

    cfo.preTransform   = surface_caps.currentTransform;
    cfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    cfo.presentMode    = vk::PresentModeKHR::eFifo;
    cfo.clipped        = VK_TRUE;

    swapchain        = dev->createSwapchainKHRUnique(cfo);
    swapchain_images = dev->getSwapchainImagesKHR(swapchain.get());

    vk::ImageViewCreateInfo ivcfo{
        {},
        nullptr,
        vk::ImageViewType::e2D,
        swapchain_format,
        {},
        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
    };

    swapchain_image_views.clear();
    for(auto img : swapchain_images) {
        ivcfo.image = img;
        swapchain_image_views.emplace_back(dev->createImageViewUnique(ivcfo));
    }

    if(swapchain_images.size() != command_buffers.size()) {
        command_buffers.clear();
        command_buffers = dev->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo {
            command_pool, vk::CommandBufferLevel::ePrimary, swapchain_images.size()
        });
    }
}
