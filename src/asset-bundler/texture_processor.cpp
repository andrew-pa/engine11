#include "asset-bundler/texture_processor.h"
#include <error.h>
#include <iostream>

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

texture_processor::texture_processor() {
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
    auto err1 = instance->createDebugReportCallbackEXT(
        &cbco,
        nullptr,
        &debug_report_callback,
        vk::DispatchLoaderDynamic(instance.get(), vkGetInstanceProcAddr)
    );
    if (err1 != vk::Result::eSuccess) {
        std::cout << "WARNING: failed to create debug report callback: " << vk::to_string(err1)
            << "\n";
    }
#endif

    auto physical_devices = instance->enumeratePhysicalDevices();
    for(auto pd : physical_devices) {
        auto qufams = pd.getQueueFamilyProperties();
        for(uint32_t i = 0; i < qufams.size(); ++i) {
            if(qufams[i].queueCount > 0 && qufams[i].queueFlags & vk::QueueFlagBits::eGraphics) {
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

size_t texture_processor::submit_texture(texture_id id, int width, int height, int channels, void* data) {
    auto mip_layer_count = (uint32_t)std::floor(std::log2(std::max(width, height))) + 1;

    // create job resources
    texture_process_job s{device.get(), cmd_pool.get(), allocator, width, height, channels, mip_layer_count};

    // copy original data into staging buffer
    memcpy(s.img->cpu_mapped(), data, width*height*channels);

    // build command buffer:
    // copy from staging buffer to top of mipmap chain
    // generate rest of mipmap chain
    s.generate_mipmaps();

    // submit command buffer
    s.submit(graphics_queue);
    jobs.emplace(id, std::move(s));

    // return size of result data
    return 0;
}

texture_process_job::texture_process_job(vk::Device dev, vk::CommandPool cmd_pool, const std::shared_ptr<gpu_allocator>& alloc, int width, int height, int channels, uint32_t mip_layer_count)
    : device(dev), image_info{
        {},
            vk::ImageType::e2D,
            vk::Format(format_from_channels(channels)),
            vk::Extent3D(width, height, 1),
            mip_layer_count,
            1,
            vk::SampleCountFlagBits::e1,
            vk::ImageTiling::eLinear,
            vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst
    }
{
    // create command buffer & fence
    cmd_buffer = std::move(device.allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo {
        cmd_pool, vk::CommandBufferLevel::ePrimary, 1
    })[0]);

    fence = device.createFenceUnique(vk::FenceCreateInfo{});

    img = std::make_unique<gpu_image>(alloc,
            image_info,
            VmaAllocationCreateInfo{
                .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                .usage = VMA_MEMORY_USAGE_AUTO,
            }
        );

    cmd_buffer->begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
}

void texture_process_job::generate_mipmaps() {
    // create structures for submitting commands
    vk::ImageBlit blit_info {
        vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        {vk::Offset3D{0,0,0},{(int32_t)image_info.extent.width,(int32_t)image_info.extent.height,1}},
        vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 1, 0, 1},
        {vk::Offset3D{0,0,0},{(int32_t)image_info.extent.width/2,(int32_t)image_info.extent.height/2,1}},
    };

    vk::ImageMemoryBarrier barrierUninitToDst {
        vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eTransferWrite,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        img->get(), vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor,1,1,0,1}
    };
    vk::ImageMemoryBarrier barrierDstToSrc {
        vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead,
        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        img->get(), vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor,0,1,0,1}
    };

    // initial transition: transition the base mip level to source layout and the first new mip level to destination layout
    cmd_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
            {}, {}, {}, {
            barrierUninitToDst,
            vk::ImageMemoryBarrier {
                {}, vk::AccessFlagBits::eTransferRead,
                vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal,
                VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                img->get(), vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor,0,1,0,1}
            }
        });

    // generate each mip level by copying from the last one
    for(uint32_t i = 1; i < image_info.mipLevels; ++i) {
        cmd_buffer->blitImage(
                img->get(), vk::ImageLayout::eTransferSrcOptimal,
                img->get(), vk::ImageLayout::eTransferDstOptimal,
                1, &blit_info,
                vk::Filter::eLinear);

        // setup for the next mip level
        blit_info.srcSubresource = blit_info.dstSubresource;
        blit_info.srcOffsets = blit_info.dstOffsets;
        blit_info.dstSubresource.mipLevel++;
        blit_info.dstOffsets[1].setX(glm::max(1, blit_info.dstOffsets[1].x / 2));
        blit_info.dstOffsets[1].setY(glm::max(1, blit_info.dstOffsets[1].y / 2));

        // transition the mip level we just wrote to from destination layout to source layout so we can copy from it in the next iteration
        barrierDstToSrc.subresourceRange.setBaseMipLevel(blit_info.srcSubresource.mipLevel);
        // transition the next unwritten mip level from undefined to destination layout so we can write to it in the next iteration
        barrierUninitToDst.subresourceRange.setBaseMipLevel(blit_info.dstSubresource.mipLevel);

        cmd_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
                {}, {}, {}, {barrierUninitToDst, barrierDstToSrc});
    }
}

void texture_process_job::submit(vk::Queue queue) {
    cmd_buffer->end();
    queue.submit(vk::SubmitInfo{0, nullptr, nullptr, 1, &cmd_buffer.get()}, fence.get());
}

void texture_process_job::wait_for_completion() {
    auto err = device.waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
    if(err != vk::Result::eSuccess)
        throw vulkan_runtime_error("failed to run texture process job", err);
}

void texture_processor::recieve_processed_texture(texture_id id, void* destination) {
    auto job = std::move(jobs.extract(id).mapped());
    job.wait_for_completion();
    // copy data out of staging buffer into destination
    // clean up resources used for this texture
}
