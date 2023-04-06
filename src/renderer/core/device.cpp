#include "egg/renderer/core/frame_renderer.h"
#include "egg/renderer/renderer.h"
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
    if(!qfixs.complete()) throw std::runtime_error("failed to find sutable physical device");

    auto props = phy_dev.getProperties();
    std::cout << "using physical device " << props.deviceName << " (" << vk::to_string(props.deviceType) << ")\n";

    // create device queue create infos
    std::vector<vk::DeviceQueueCreateInfo> qu_cfo;

    auto unique_queue_families = std::unordered_set<int>{qfixs.graphics, qfixs.present};
    qu_cfo.reserve(unique_queue_families.size());
    float fp = 1.f;
    for(auto qfi : unique_queue_families)
        qu_cfo.emplace_back(vk::DeviceQueueCreateFlags{}, (uint32_t)qfi, 1, &fp);

    // create device create info
    vk::PhysicalDeviceFeatures device_features;

    const char* layer_names[] = {
#ifndef NDEBUG
        "VK_LAYER_LUNARG_standard_validation",
#endif
    };
    uint32_t layer_count =
#ifndef NDEBUG
        1
#else
        0
#endif
        ;

    const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    dev = phy_dev.createDeviceUnique(vk::DeviceCreateInfo{
        {}, (uint32_t)qu_cfo.size(), qu_cfo.data(), layer_count, layer_names, 1, extensions, &device_features});

    VmaAllocatorCreateInfo cfo = {};
    cfo.instance               = instance;
    cfo.physicalDevice         = phy_dev;
    cfo.device                 = dev.get();
    auto err                   = vmaCreateAllocator(&cfo, &allocator);
    if(err != VK_SUCCESS) throw std::runtime_error("failed to create GPU allocator: " + std::to_string(err));

    graphics_queue = dev->getQueue(qfixs.graphics, 0);
    present_queue  = dev->getQueue(qfixs.present, 0);

    graphics_queue_family_index = qfixs.graphics;
    present_queue_family_index  = qfixs.present;
}
