#include "egg/renderer/core/frame_renderer.h"
#include "egg/renderer/renderer.h"
#include "error.h"
#include <egg/renderer/memory.h>
#include <iostream>
#include <unordered_set>

struct queue_family_indices {
    int graphics = -1;
    int present  = -1;

    void reset() {
        graphics = -1;
        present  = -1;
    }

    void gather(vk::PhysicalDevice pd, vk::SurfaceKHR surface) {
        auto qufams = pd.getQueueFamilyProperties();
        for(uint32_t i = 0; i < qufams.size() && !complete(); ++i) {
            if(qufams[i].queueCount <= 0) continue;
            if(qufams[i].queueFlags & vk::QueueFlagBits::eGraphics) graphics = i;
            if(pd.getSurfaceSupportKHR(i, surface) != 0u) present = i;
        }
    }

    bool complete() const { return graphics >= 0 && present >= 0; }

    std::vector<vk::DeviceQueueCreateInfo> generate_create_infos() {
        std::vector<vk::DeviceQueueCreateInfo> qu_cfo;

        auto unique_queue_families = std::unordered_set<int>{graphics, present};
        qu_cfo.reserve(unique_queue_families.size());
        float fp = 1.f;
        for(auto qfi : unique_queue_families)
            qu_cfo.emplace_back(vk::DeviceQueueCreateFlags{}, (uint32_t)qfi, 1, &fp);
        return qu_cfo;
    }
};

void renderer::init_device(vk::Instance instance) {
    queue_family_indices qfixs;

    // find physical device and queue family indices
    auto physical_devices = instance.enumeratePhysicalDevices();
    for(auto physical_device : physical_devices) {
        qfixs.gather(physical_device, window_surface.get());
        if(qfixs.complete()) {
            phy_dev = physical_device;
            break;
        }
    }
    if(!qfixs.complete()) throw std::runtime_error("failed to find suitable physical device");

    auto props = phy_dev.getProperties();
    std::cout << "using physical device " << props.deviceName << " ("
              << vk::to_string(props.deviceType) << ")\n";

    // create device create info
    VkPhysicalDeviceVulkan12Features v12_features{
        .sType                  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .runtimeDescriptorArray = VK_TRUE
    };
    VkPhysicalDeviceVulkan11Features v11_features{
        .sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext                 = &v12_features,
        .storagePushConstant16 = VK_TRUE
    };
    vk::PhysicalDeviceFeatures2 device_features{{}, &v11_features};

    const char* layer_names[] = {
#ifndef NDEBUG
        "VK_LAYER_KHRONOS_validation",
#endif
        ""
    };
    uint32_t layer_count =
#ifndef NDEBUG
        1
#else
        0
#endif
        ;

    const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    // create device queue create infos
    std::vector<vk::DeviceQueueCreateInfo> qu_cfo = qfixs.generate_create_infos();

    dev = phy_dev.createDeviceUnique(vk::DeviceCreateInfo{
        {},
        (uint32_t)qu_cfo.size(),
        qu_cfo.data(),
        layer_count,
        layer_names,
        1,
        extensions,
        nullptr,
        &device_features
    });

    VmaAllocatorCreateInfo cfo = {
        .physicalDevice = phy_dev,
        .device         = dev.get(),
        .instance       = instance,
    };
    allocator = create_allocator(cfo);

    graphics_queue = dev->getQueue(qfixs.graphics, 0);
    present_queue  = dev->getQueue(qfixs.present, 0);

    graphics_queue_family_index = qfixs.graphics;
    present_queue_family_index  = qfixs.present;
}
