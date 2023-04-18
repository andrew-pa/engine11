#pragma once
#include "egg/renderer/renderer.h"
#include <GLFW/glfw3.h>
#include <functional>
#include <memory>
#include <vulkan/vulkan.hpp>

struct frame {
    uint32_t          frame_index;
    vk::CommandBuffer frame_cmd_buf;
};

class frame_renderer {
    renderer*                        r;
    vk::UniqueSwapchainKHR           swapchain;
    std::vector<vk::Image>           swapchain_images;
    std::vector<vk::UniqueImageView> swapchain_image_views;
    vk::Extent2D                     swapchain_extent;
    vk::UniqueSemaphore              image_available, render_finished;
    void                             init_swapchain();

    std::vector<vk::UniqueCommandBuffer> command_buffers;

  public:
    frame_renderer(renderer* r, vk::Extent2D swapchain_extent);

    void                               reset_swapchain(vk::Extent2D new_swapchain_extent);
    std::vector<vk::UniqueFramebuffer> create_framebuffers(
        vk::RenderPass                                                  render_pass,
        const std::function<void(size_t, std::vector<vk::ImageView>&)>& custom_image_views
        = [](auto, auto) {}
    );

    frame begin_frame();
    void  end_frame(frame&& frame);

    inline vk::Extent2D extent() { return swapchain_extent; }
};
