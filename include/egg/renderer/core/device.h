#pragma once
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace vkx {

struct device {
    vk::UniqueDevice   dev;
    vk::PhysicalDevice phy_dev;
    vk::Queue          graphics_queue, present_queue;
    VmaAllocator       allocator;

    device(vk::Instance instance, vk::SurfaceKHR surface);
};

}  // namespace vkx
