#pragma once
#include <vulkan/vulkan.hpp>

namespace vkx {

class device {
  public:
    device(vk::Instance instance, vk::SurfaceKHR surface);
};

}  // namespace vkx
