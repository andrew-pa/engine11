#include "asset-bundler/texture_process_jobs.h"
#include "asset-bundler/texture_processor.h"
#include <vulkan/vulkan_format_traits.hpp>

inline vk::Format format_from_channels_f32(int nchannels) {
    switch(nchannels) {
        case 1: return vk::Format::eR32Sfloat;
        case 2: return vk::Format::eR32G32Sfloat;
        case 3: return vk::Format::eR32G32B32Sfloat;
        case 4: return vk::Format::eR32G32B32A32Sfloat;
        default:
            throw std::runtime_error(
                "invalid number of channels to convert to Vulkan format: "
                + std::to_string(nchannels)
            );
    }
}

environment_process_job::environment_process_job(vk::Device dev, vk::CommandPool cmd_pool, const std::shared_ptr<gpu_allocator>& alloc, environment_info* info,
        uint32_t src_width, uint32_t src_height, int src_nchannels, float* src_data)
    : process_job(dev, cmd_pool)
{
    // create image descriptions
    vk::ImageCreateInfo src_image_info {
        {},
        vk::ImageType::e2D,
        format_from_channels_f32(src_nchannels),
        vk::Extent3D{src_width, src_height, 1},
        1, 1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst
    };

    auto sky_image_info = info->skybox.vulkan_create_info(vk::ImageUsageFlagBits::eTransferDst);

    // compute total input/output size of the job
    auto in_size = linear_image_size_in_bytes(src_image_info);
    auto out_size = linear_image_size_in_bytes(sky_image_info);

    total_output_size = info->len = out_size;

    // create source GPU objects
    src = std::make_unique<gpu_image>(alloc, src_image_info);
    init_staging_buffer(alloc, std::max(in_size, out_size));

    // copy source environment onto GPU
    memcpy(staging->cpu_mapped(), src_data, in_size);

    // create destination GPU objects
    skybox = std::make_unique<gpu_image>(alloc, sky_image_info);

    // create shaders, pipelines, descriptors, etc.

    cmd_buffer->begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    // transition the source image to transfer destination to prepare to recieve staging buffer data
    cmd_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
            {}, {}, {}, {
            vk::ImageMemoryBarrier {
                vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eTransferWrite,
                vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                src->get(),
                vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor,0,1,0,1}
            }
        });

    // copy source environment map data from staging buffer to the source image
    cmd_buffer->copyBufferToImage(staging->get(), src->get(),
            vk::ImageLayout::eTransferDstOptimal,
            vk::BufferImageCopy {
                0, 0, 0,
                vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                vk::Offset3D{0,0,0},
                src_image_info.extent
            });

    // run skybox generation shader
    // copy cubemap into staging buffer
}

const vk::DescriptorSetLayoutBinding desc_set_bindings[] = {
    // input environment map texture
    {0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute},
    // output skybox cubemap
    {0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute},
};

environment_process_job_resources::environment_process_job_resources(vk::Device dev)
    : desc_set_layout(dev.createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo{
        {},
        sizeof(desc_set_bindings)/sizeof(desc_set_bindings[0]),
        desc_set_bindings
    }))

{
}

