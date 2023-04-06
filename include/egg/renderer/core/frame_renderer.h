#pragma once
#include <memory>
#define GLFW_INCLUDE_VULKAN
#include "egg/renderer/core/swap_chain.h"
#include <GLFW/glfw3.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

struct frame {
    uint32_t          frame_index;
    vk::CommandBuffer frame_cmd_buf;
};

class frame_renderer {
    vk::UniqueDevice   dev;
    vk::PhysicalDevice phy_dev;
    uint32_t graphics_queue_family_index, present_queue_family_index;
    vk::Queue          graphics_queue, present_queue;
    VmaAllocator       allocator;
    void               init_device(vk::Instance instance);

    vk::UniqueSurfaceKHR             window_surface;
    vk::UniqueSwapchainKHR           swapchain;
    std::vector<vk::Image>           swapchain_images;
    std::vector<vk::UniqueImageView> swapchain_image_views;
    vk::Extent2D                     swapchain_extent;
    vk::Format                       swapchain_format;
    vk::UniqueSemaphore              image_available, render_finished;
    void                             init_swapchain();

    vk::UniqueCommandPool command_pool;
    std::vector<vk::UniqueCommandBuffer> command_buffers;

  public:
    frame_renderer(GLFWwindow* window, vk::Instance instance);

    void reset_swapchain();

    frame begin_frame();
    void  end_frame(frame&& frame);
};
