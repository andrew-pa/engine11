#pragma once
#include <memory>
#define GLFW_INCLUDE_VULKAN
#include "egg/renderer/core/device.h"
#include "egg/renderer/core/swap_chain.h"
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

struct frame {};

class frame_renderer {
    vk::UniqueSurfaceKHR             window_surface;
    std::unique_ptr<vkx::device>     device;
    std::unique_ptr<vkx::swap_chain> swap_chain;

  public:
    frame_renderer(GLFWwindow* window, vk::Instance instance);

    frame begin_frame();
    void  end_frame(frame&& frame);
};
