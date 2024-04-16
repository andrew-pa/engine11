#pragma once
#include "vk.h"

struct vulkan_runtime_error : public std::runtime_error {
    vk::Result reason;

    vulkan_runtime_error(const std::string& message, vk::Result reason)
        : std::runtime_error(message + " (cause: " + vk::to_string(reason) + ")"), reason(reason) {}

    vulkan_runtime_error(const std::string& message, VkResult reason)
        : vulkan_runtime_error(message, vk::Result(reason)) {}
};
