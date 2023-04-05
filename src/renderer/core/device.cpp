#include "egg/renderer/core/device.h"
#include <iostream>

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

namespace vkx {

device::device(vk::Instance instance, vk::SurfaceKHR surface) {
    queue_family_indices qfixs;

    auto physical_devices = instance.enumeratePhysicalDevices();
    for(auto physical_device : physical_devices) {
        qfixs.gather(physical_device, surface);
        if(qfixs.complete()) {
            phy_dev = physical_device;
            break;
        }
    }
    if(!qfixs.complete()) throw std::runtime_error("failed to find sutable physical device");

    auto props = phy_dev.getProperties();
    std::cout << "using physical device " << props.deviceName << "\n";
}

}  // namespace vkx
