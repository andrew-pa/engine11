#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

struct frame {};

class frame_renderer {
    vk::UniqueSurfaceKHR window_surface;

  public:
    frame_renderer(GLFWwindow* window, vk::Instance instance);

    frame begin_frame();
    void  end_frame(frame&& frame);
};
