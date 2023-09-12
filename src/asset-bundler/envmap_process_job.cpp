#include "asset-bundler/texture_process_jobs.h"
#include "asset-bundler/texture_processor.h"
#include <error.h>
#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_structs.hpp>

const size_t max_concurrent_jobs = 8;

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

environment_process_job::environment_process_job(vk::Device dev, vk::CommandPool cmd_pool,
        environment_process_job_resources* res,
        const std::shared_ptr<gpu_allocator>& alloc, environment_info* info,
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
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled
    };

    auto sky_image_info = info->skybox.vulkan_create_info(
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc,
        vk::ImageCreateFlagBits::eCubeCompatible
    );

    // compute total input/output size of the job
    auto in_size = linear_image_size_in_bytes(src_image_info);
    auto out_size = linear_image_size_in_bytes(sky_image_info);

    total_output_size = info->len = out_size;

    // create source GPU objects
    src = std::make_unique<gpu_image>(alloc, src_image_info);
    init_staging_buffer(alloc, std::max(in_size, out_size));

    src_view = dev.createImageViewUnique(vk::ImageViewCreateInfo {
        {},
        src->get(),
        vk::ImageViewType::e2D,
        src_image_info.format,
        vk::ComponentMapping{},
        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
    });

    // copy source environment onto GPU
    memcpy(staging->cpu_mapped(), src_data, in_size);

    // create destination GPU objects
    skybox = std::make_unique<gpu_image>(alloc, sky_image_info);
    skybox_view = dev.createImageViewUnique(vk::ImageViewCreateInfo {
        {},
        skybox->get(),
        vk::ImageViewType::eCube,
        sky_image_info.format,
        vk::ComponentMapping{},
        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, sky_image_info.arrayLayers}
    });

    // create descriptor for resources
    desc_set = std::move(dev.allocateDescriptorSetsUnique(vk::DescriptorSetAllocateInfo {
        res->desc_pool.get(), res->desc_set_layout.get()
    })[0]);


    vk::DescriptorImageInfo src_desc_info { res->sampler.get(), src_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal };
    vk::DescriptorImageInfo sky_desc_info { VK_NULL_HANDLE, skybox_view.get(), vk::ImageLayout::eGeneral };
    dev.updateDescriptorSets({
        vk::WriteDescriptorSet{desc_set.get(), 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &src_desc_info},
        vk::WriteDescriptorSet{desc_set.get(), 1, 0, 1, vk::DescriptorType::eStorageImage, &sky_desc_info}
    }, {});

    this->build_cmd_buffer(cmd_buffer.get(), res, src_image_info, sky_image_info);
}

void environment_process_job::build_cmd_buffer(
        vk::CommandBuffer cmd_buffer,
        environment_process_job_resources* res,
        // TODO: should these just be fields in the job struct?
        const vk::ImageCreateInfo& src_image_info,
        const vk::ImageCreateInfo& sky_image_info
) {
    cmd_buffer.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    // transition the source image to transfer destination to prepare to recieve staging buffer data
    cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
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
    cmd_buffer.copyBufferToImage(staging->get(), src->get(),
            vk::ImageLayout::eTransferDstOptimal,
            vk::BufferImageCopy {
                0, 0, 0,
                vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                vk::Offset3D{0,0,0},
                src_image_info.extent
            });

    // wait for the copy to finish and then transition the source image so we can read from it in the shader
    // also move the output cubemap into general layout so we can write to it from the shader
    cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader,
            {}, {}, {}, {
            vk::ImageMemoryBarrier {
                vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
                vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                src->get(),
                vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor,0,1,0,1}
            },
            vk::ImageMemoryBarrier {
                {}, vk::AccessFlagBits::eShaderWrite,
                vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral,
                VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                skybox->get(),
                vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor,0,1,0,sky_image_info.arrayLayers}
            }
        });

    // run skybox generation shader
    cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, res->pipeline_layout.get(), 0, desc_set.get(), {});
    cmd_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, res->skybox_pipeline.get());
    const uint32_t SKYBOX_SHADER_LOCAL_SIZE = 64;
    cmd_buffer.dispatch(
            sky_image_info.extent.width  / SKYBOX_SHADER_LOCAL_SIZE,
            sky_image_info.extent.height / SKYBOX_SHADER_LOCAL_SIZE,
            sky_image_info.arrayLayers);

    // TODO: generate mipmaps for cubemaps

    // transition skybox to transfer src so we can copy it out
    cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer,
            {}, {}, { }, {
                vk::ImageMemoryBarrier {
                    vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead,
                    vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal,
                    VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                    skybox->get(),
                    vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor,0,1,0,sky_image_info.arrayLayers}
                }
        });

    // copy cubemap into staging buffer
    std::vector<vk::BufferImageCopy> regions;
    uint32_t w = sky_image_info.extent.width, h = sky_image_info.extent.height;
    size_t offset = 0;
    for(uint32_t i = 0; i < sky_image_info.arrayLayers; ++i) {
        regions.emplace_back(vk::BufferImageCopy{
                offset,
                0, 0, // tightly packex
                vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, i, 1},
                vk::Offset3D{0,0,0},
                vk::Extent3D{w,h,1}
            });
        offset += w * h * vk::blockSize(sky_image_info.format);
    }
    cmd_buffer.copyImageToBuffer(
            skybox->get(),
            vk::ImageLayout::eTransferSrcOptimal,
            staging->get(),
            regions);

}

const vk::DescriptorSetLayoutBinding desc_set_bindings[] = {
    // input environment map texture
    {0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute},
    // output skybox cubemap
    {1, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute},
};

const uint32_t skybox_shader_bytecode[] = {
#include "skybox.comp.num"
};
const vk::ShaderModuleCreateInfo skybox_shader_create_info{ {}, sizeof(skybox_shader_bytecode), skybox_shader_bytecode };

environment_process_job_resources::environment_process_job_resources(vk::Device dev) {
    desc_set_layout = dev.createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo{
        {},
        sizeof(desc_set_bindings)/sizeof(desc_set_bindings[0]),
        desc_set_bindings
    });

    const vk::DescriptorPoolSize pool_sizes[] {
        vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, max_concurrent_jobs},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageImage, max_concurrent_jobs},
    };

    desc_pool = dev.createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo {
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, max_concurrent_jobs,
        sizeof(pool_sizes)/sizeof(pool_sizes[0]), pool_sizes
    });

    pipeline_layout = dev.createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo {
        {}, desc_set_layout.get(), {}
    });

    auto skybox_shader_module = dev.createShaderModuleUnique(skybox_shader_create_info);

    auto res = dev.createComputePipelineUnique(VK_NULL_HANDLE, vk::ComputePipelineCreateInfo {
            {},
            vk::PipelineShaderStageCreateInfo {{}, vk::ShaderStageFlagBits::eCompute, skybox_shader_module.get(), "main"},
            pipeline_layout.get(),
    });
    if(res.result != vk::Result::eSuccess)
        throw vulkan_runtime_error("failed to create skybox pipeline", res.result);
    skybox_pipeline = std::move(res.value);

    sampler = dev.createSamplerUnique(vk::SamplerCreateInfo{{}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear});
}

