#include "asset-bundler/texture_processor.h"
#include "asset-bundler/texture_process_jobs.h"
#include <error.h>
#include <iostream>
#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vk_extension_helper.h>

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugReportFlagsEXT      flags,
    VkDebugReportObjectTypeEXT objType,
    uint64_t                   obj,
    size_t                     location,
    int32_t                    code,
    const char*                layerPrefix,
    const char*                msg,
    void*                      userData
) {
    if(std::strncmp("Device Extension", msg, 16) == 0) return VK_FALSE;
    vk::DebugReportFlagsEXT flg{(vk::DebugReportFlagBitsEXT)flags};
    if(flg & vk::DebugReportFlagBitsEXT::eDebug)
        std::cout << "[DEBUG]";
    else if(flg & vk::DebugReportFlagBitsEXT::eError)
        std::cout << "\n[\033[31mERROR\033[0m]";
    else if(flg & vk::DebugReportFlagBitsEXT::eInformation)
        std::cout << "[\033[32mINFO\033[0m]";
    else if(flg & vk::DebugReportFlagBitsEXT::ePerformanceWarning)
        std::cout << "[PERF]";
    else if(flg & vk::DebugReportFlagBitsEXT::eWarning)
        std::cout << "[\033[33mWARN\033[0m]";
    std::cout << ' ' /*<< layerPrefix << " | "*/ << msg << "\n";
    // throw code;
    return VK_FALSE;
}

const vk::ApplicationInfo APP_INFO = vk::ApplicationInfo{
    "asset-bundler", VK_MAKE_VERSION(0, 0, 0), "egg", VK_MAKE_VERSION(0, 0, 0), VK_API_VERSION_1_3};

texture_processor::texture_processor()
    : env_res(nullptr)
{
    std::vector<const char*> extensions;
    //extensions.push_back("VK_KHR_portability_enumeration");
#ifndef NDEBUG
    extensions.emplace_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif
    instance = vk::createInstanceUnique(vk::InstanceCreateInfo {
        vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR,
        &APP_INFO,
        0, {},
        (uint32_t)extensions.size(),
        extensions.data()
    });

    // set up vulkan debugging reports
#ifndef NDEBUG
    auto cbco = vk::DebugReportCallbackCreateInfoEXT{
        vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eDebug
            | vk::DebugReportFlagBitsEXT::ePerformanceWarning | vk::DebugReportFlagBitsEXT::eWarning
            | vk::DebugReportFlagBitsEXT::eInformation,
        debug_callback };
    debug_report_callback = instance->createDebugReportCallbackEXT(
        cbco,
        nullptr,
        vk::DispatchLoaderDynamic(instance.get(), vkGetInstanceProcAddr)
    );
#endif

    auto physical_devices = instance->enumeratePhysicalDevices();
    for(auto pd : physical_devices) {
        auto qufams = pd.getQueueFamilyProperties();
        for(uint32_t i = 0; i < qufams.size(); ++i) {
            // TODO: right now we only support a single queue with graphcs and compute support
            if(qufams[i].queueCount > 0 && qufams[i].queueFlags & vk::QueueFlagBits::eGraphics && qufams[i].queueFlags & vk::QueueFlagBits::eCompute) {
                graphics_queue_family_index = i;
                phy_device = pd;
                break;
            }
        }
    }
    if(!phy_device) throw std::runtime_error("failed to find suitable physical device");
    auto props = phy_device.getProperties();
    std::cout << "using physical device " << props.deviceName << " ("
              << vk::to_string(props.deviceType) << ")\n";

    float fp = 1.f;
    auto queue_create_info = vk::DeviceQueueCreateInfo { {}, graphics_queue_family_index, 1, &fp };

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

    auto device_features = vk::PhysicalDeviceFeatures{};

    device = phy_device.createDeviceUnique(vk::DeviceCreateInfo {
        {},
        1,
        &queue_create_info,
        layer_count,
        layer_names,
        0,
        nullptr,
        &device_features
    });

    VmaAllocatorCreateInfo cfo = {
        .physicalDevice = phy_device,
        .device = device.get(),
        .instance = instance.get(),
    };
    allocator = create_allocator(cfo);

    graphics_queue = device->getQueue(graphics_queue_family_index, 0);

    cmd_pool = device->createCommandPoolUnique(vk::CommandPoolCreateInfo{
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer, graphics_queue_family_index });
}

texture_processor::~texture_processor() {
    assert(jobs.empty());
    env_res.reset();
    allocator.reset();
    cmd_pool.reset();
    device.reset();
    instance->destroyDebugReportCallbackEXT(debug_report_callback, nullptr,
        vk::DispatchLoaderDynamic(instance.get(), vkGetInstanceProcAddr));
    instance.reset();
}

void texture_processor::submit_texture(texture_id id, texture_info* info) {
    info->img.mip_levels = (uint32_t)std::floor(std::log2(std::max(info->img.width, info->img.height))) + 1;

    if (info->img.mip_levels == 1) {
        // no reason to generate mip levels for this texture at all
        info->len = info->img.width * vk::blockSize(info->img.format) * info->img.height;
        return;
    }

    texture_process_job s{device.get(), cmd_pool.get(), allocator, info};

    // free CPU image data and mark that this image has been copied to the GPU
    free(info->data);
    info->data = nullptr;

    s.generate_mipmaps();

    s.submit(graphics_queue);
    info->len = s.total_output_size;
    jobs.emplace(id, std::move(s));
}

void texture_processor::recieve_processed_texture(texture_id id, void* destination) {
    auto job = std::move(jobs.extract(id).mapped());
    // copy data out of staging buffer into destination
    job.wait_for_completion((uint8_t*)destination);
    // clean up resources used for this texture
}

environment_info texture_processor::submit_environment(string_id name, uint32_t width, uint32_t height, int nchannels, float* data) {
    environment_info info{
        .name = name,
        // TODO: these are fixed but should be based on some kind of quality level
        .skybox = image_info {2048, 2048, 1, 6, vk::Format::eR8G8B8A8Unorm}
    };

    if(env_res == nullptr) {
        env_res = std::make_unique<environment_process_job_resources>(device.get());
    }

    environment_process_job s{
        device.get(),
        cmd_pool.get(),
        env_res.get(),
        allocator,
        &info,
        width, height, nchannels, data
    };

    // free CPU source data
    free(data);

    s.submit(graphics_queue);
    env_jobs.emplace(name, std::move(s));

    return info;
}

void texture_processor::recieve_processed_environment(string_id name, void* destination) {
    auto job = std::move(env_jobs.extract(name).mapped());
    // copy data out of staging buffer into destination
    job.wait_for_completion((uint8_t*)destination);
    // clean up resources used for this texture
}

size_t linear_image_size_in_bytes(const vk::ImageCreateInfo& image_info) {
    size_t total_size = 0;
    uint32_t w = image_info.extent.width, h = image_info.extent.height;
    for(auto mi = 0; mi < image_info.mipLevels; ++mi) {
        total_size += w * h * vk::blockSize(image_info.format);
        w = glm::max(w/2, 1u); h = glm::max(h/2, 1u);
    }
    total_size *= image_info.arrayLayers;
    return total_size;
}


