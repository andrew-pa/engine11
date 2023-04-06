#pragma once
#include "egg/renderer/renderer.h"
#include <GLFW/glfw3.h>
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
    vk::Format                       swapchain_format;
    vk::UniqueSemaphore              image_available, render_finished;
    void                             init_swapchain();

    vk::UniqueCommandPool                command_pool;
    std::vector<vk::UniqueCommandBuffer> command_buffers;

  public:
    frame_renderer(renderer* r);

    void reset_swapchain();

    frame begin_frame();
    void  end_frame(frame&& frame);
};
