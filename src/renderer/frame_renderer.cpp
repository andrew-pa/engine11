#include "egg/renderer/frame_renderer.h"

frame_renderer::frame_renderer(GLFWwindow* window, vk::Instance instance) {
    // create window surface
    VkSurfaceKHR surface;

    auto err = glfwCreateWindowSurface(instance, window, nullptr, &surface);
    if(err != VK_SUCCESS) throw std::runtime_error("failed to create window surface " + std::to_string(err));

    window_surface = vk::UniqueSurfaceKHR(surface);

    // create device & swap chain
}

frame frame_renderer::begin_frame() { return frame{}; }

void frame_renderer::end_frame(frame&& frame) {}
