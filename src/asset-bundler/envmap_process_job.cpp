#include "asset-bundler/texture_process_jobs.h"
#include "asset-bundler/texture_processor.h"
#include <error.h>
#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_structs.hpp>

const size_t max_concurrent_jobs = 16;

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

environment_process_job::environment_process_job(
    vk::Device                            dev,
    vk::CommandPool                       cmd_pool,
    environment_process_job_resources*    res,
    const std::shared_ptr<gpu_allocator>& alloc,
    environment_info*                     info,
    uint32_t                              src_width,
    uint32_t                              src_height,
    int                                   src_nchannels,
    float*                                src_data,
    bool                                  enable_ibl_precomp
)
    : process_job(dev, cmd_pool) {
    // create image descriptions
    src_image_info = vk::ImageCreateInfo{
        {},
        vk::ImageType::e2D,
        format_from_channels_f32(src_nchannels),
        vk::Extent3D{src_width, src_height, 1},
        1,
        1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst
            | vk::ImageUsageFlagBits::eSampled
    };

    sky_image_info = info->skybox.vulkan_create_info(
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc,
        vk::ImageCreateFlagBits::eCubeCompatible
    );

    diffuse_map_image_info = info->diffuse_irradiance.vulkan_create_info(
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc,
        vk::ImageCreateFlagBits::eCubeCompatible
    );

    // compute total input/output size of the job
    auto in_size     = linear_image_size_in_bytes(src_image_info);
    auto skybox_size = linear_image_size_in_bytes(sky_image_info);
    auto out_size    = skybox_size + linear_image_size_in_bytes(diffuse_map_image_info);

    info->diffuse_irradiance_offset = skybox_size;

    total_output_size = info->len = out_size;

    // create source GPU objects
    src = std::make_unique<gpu_image>(alloc, src_image_info);
    init_staging_buffer(alloc, std::max(in_size, out_size));

    src_view = dev.createImageViewUnique(vk::ImageViewCreateInfo{
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
    skybox      = std::make_unique<gpu_image>(alloc, sky_image_info);
    skybox_view = dev.createImageViewUnique(
        info->skybox.vulkan_full_image_view(skybox->get(), vk::ImageViewType::eCube)
    );

    diffuse_map      = std::make_unique<gpu_image>(alloc, diffuse_map_image_info);
    diffuse_map_view = dev.createImageViewUnique(info->diffuse_irradiance.vulkan_full_image_view(
        diffuse_map->get(), vk::ImageViewType::eCube
    ));

    // create descriptor for resources
    vk::DescriptorSetLayout desc_set_layouts[] = {
        res->desc_set_layout.get(),
        res->desc_set_layout.get(),
    };
    desc_sets = dev.allocateDescriptorSetsUnique(
        vk::DescriptorSetAllocateInfo{res->desc_pool.get(), 2, desc_set_layouts}
    );

    vk::DescriptorImageInfo src_desc_info{
        res->sampler.get(), src_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal
    };
    vk::DescriptorImageInfo sky_desc_info{
        VK_NULL_HANDLE, skybox_view.get(), vk::ImageLayout::eGeneral
    };
    vk::DescriptorImageInfo diffuse_map_desc_info{
        VK_NULL_HANDLE, diffuse_map_view.get(), vk::ImageLayout::eGeneral
    };
    // set output for each compute job differently
    auto writes = std::vector<vk::WriteDescriptorSet>{
        vk::WriteDescriptorSet{
                               desc_sets[0].get(), 1, 0, 1, vk::DescriptorType::eStorageImage, &sky_desc_info
        },
        vk::WriteDescriptorSet{
                               desc_sets[1].get(), 1, 0, 1, vk::DescriptorType::eStorageImage, &diffuse_map_desc_info
        }
    };
    // input for all compute jobs is the same
    for(const auto& d : desc_sets) {
        writes.emplace_back(
            d.get(), 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &src_desc_info
        );
    }
    dev.updateDescriptorSets(writes, {});

    this->build_cmd_buffer(cmd_buffer.get(), res);
}

void environment_process_job::build_cmd_buffer(
    vk::CommandBuffer cmd_buffer, environment_process_job_resources* res
) {
    cmd_buffer.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    // transition the source image to transfer destination to prepare to recieve staging buffer data
    cmd_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer,
        {
    },
        {},
        {},
        {vk::ImageMemoryBarrier{
            vk::AccessFlagBits::eTransferRead,
            vk::AccessFlagBits::eTransferWrite,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            src->get(),
            vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
        }}
    );

    // copy source environment map data from staging buffer to the source image
    cmd_buffer.copyBufferToImage(
        staging->get(),
        src->get(),
        vk::ImageLayout::eTransferDstOptimal,
        vk::BufferImageCopy{
            0,
            0,
            0,
            vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
            vk::Offset3D{0, 0, 0},
            src_image_info.extent
    }
    );

    // wait for the copy to finish and then transition the source image so we can read from it in
    // the shader
    std::vector<vk::ImageMemoryBarrier> barriers{
        vk::ImageMemoryBarrier{
                               vk::AccessFlagBits::eTransferWrite,
                               vk::AccessFlagBits::eShaderRead,
                               vk::ImageLayout::eTransferDstOptimal,
                               vk::ImageLayout::eShaderReadOnlyOptimal,
                               VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                               src->get(),
                               vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
        }
    };
    // also move the output cubemaps into general layout so we can write to it from the shader
    for(auto i : {skybox->get(), diffuse_map->get()}) {
        barriers.emplace_back(vk::ImageMemoryBarrier{
            {},
            vk::AccessFlagBits::eShaderWrite,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eGeneral,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            i,
            vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 6}
        });
    }
    cmd_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eComputeShader,
        {},
        {},
        {},
        barriers
    );

    // run skybox generation shader
    cmd_buffer.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute, res->pipeline_layout.get(), 0, desc_sets[0].get(), {}
    );
    cmd_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, res->skybox_pipeline.get());
    const uint32_t SKYBOX_SHADER_LOCAL_SIZE = 32;
    cmd_buffer.dispatch(
        sky_image_info.extent.width / SKYBOX_SHADER_LOCAL_SIZE,
        sky_image_info.extent.height / SKYBOX_SHADER_LOCAL_SIZE,
        sky_image_info.arrayLayers
    );

    // run diffuse irradiance map generation shader
    cmd_buffer.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute, res->pipeline_layout.get(), 0, desc_sets[1].get(), {}
    );
    cmd_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, res->diffuse_map_pipeline.get());
    const uint32_t DIFFUSE_MAP_SHADER_LOCAL_SIZE = 32;
    cmd_buffer.dispatch(
        diffuse_map_image_info.extent.width / DIFFUSE_MAP_SHADER_LOCAL_SIZE,
        diffuse_map_image_info.extent.height / DIFFUSE_MAP_SHADER_LOCAL_SIZE,
        diffuse_map_image_info.arrayLayers
    );

    // TODO: generate mipmaps for cubemaps

    // transition skybox to transfer src so we can copy it out
    barriers.clear();
    for(auto i : {skybox->get(), diffuse_map->get()}) {
        barriers.emplace_back(vk::ImageMemoryBarrier{
            vk::AccessFlagBits::eShaderWrite,
            vk::AccessFlagBits::eTransferRead,
            vk::ImageLayout::eGeneral,
            vk::ImageLayout::eTransferSrcOptimal,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            i,
            vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 6}
        });
    }
    cmd_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eTransfer,
        {},
        {},
        {},
        barriers
    );

    // copy cubemap into staging buffer
    size_t offset  = 0;
    auto   regions = copy_regions_for_linear_image2d(
        sky_image_info.extent.width,
        sky_image_info.extent.height,
        sky_image_info.mipLevels,
        sky_image_info.arrayLayers,
        sky_image_info.format,
        offset
    );
    cmd_buffer.copyImageToBuffer(
        skybox->get(), vk::ImageLayout::eTransferSrcOptimal, staging->get(), regions
    );

    regions = copy_regions_for_linear_image2d(
        diffuse_map_image_info.extent.width,
        diffuse_map_image_info.extent.height,
        diffuse_map_image_info.mipLevels,
        diffuse_map_image_info.arrayLayers,
        diffuse_map_image_info.format,
        offset
    );
    cmd_buffer.copyImageToBuffer(
        diffuse_map->get(), vk::ImageLayout::eTransferSrcOptimal, staging->get(), regions
    );
}

const vk::DescriptorSetLayoutBinding desc_set_bindings[] = {
  // input environment map texture
    {0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute},
 // output cubemap
    {1, vk::DescriptorType::eStorageImage,         1, vk::ShaderStageFlagBits::eCompute},
};

const uint32_t skybox_shader_bytecode[] = {
#include "skybox.comp.num"
};
const vk::ShaderModuleCreateInfo skybox_shader_create_info{
    {}, sizeof(skybox_shader_bytecode), skybox_shader_bytecode
};

const uint32_t diffuse_map_shader_bytecode[] = {
#include "diffuse_irradiance_map.comp.num"
};
const vk::ShaderModuleCreateInfo diffuse_map_shader_create_info{
    {}, sizeof(diffuse_map_shader_bytecode), diffuse_map_shader_bytecode
};

vk::UniquePipeline create_compute_pipeline(
    vk::Device dev, vk::PipelineLayout pipeline_layout, vk::ShaderModule shader
) {
    auto res = dev.createComputePipelineUnique(
        VK_NULL_HANDLE,
        vk::ComputePipelineCreateInfo{
            {},
            vk::PipelineShaderStageCreateInfo{
             {}, vk::ShaderStageFlagBits::eCompute, shader, "main"
            },
            pipeline_layout,
    }
    );
    if(res.result != vk::Result::eSuccess)
        throw vulkan_runtime_error("failed to create skybox pipeline", res.result);
    return std::move(res.value);
}

environment_process_job_resources::environment_process_job_resources(vk::Device dev) {
    desc_set_layout = dev.createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo{
        {}, sizeof(desc_set_bindings) / sizeof(desc_set_bindings[0]), desc_set_bindings
    });

    const vk::DescriptorPoolSize pool_sizes[]{
        vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, max_concurrent_jobs},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageImage,         max_concurrent_jobs},
    };

    desc_pool = dev.createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo{
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        max_concurrent_jobs,
        sizeof(pool_sizes) / sizeof(pool_sizes[0]),
        pool_sizes
    });

    pipeline_layout
        = dev.createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo{{}, desc_set_layout.get(), {}}
        );

    auto skybox_shader_module = dev.createShaderModuleUnique(skybox_shader_create_info);
    skybox_pipeline
        = create_compute_pipeline(dev, pipeline_layout.get(), skybox_shader_module.get());

    auto diffuse_map_shader_module = dev.createShaderModuleUnique(diffuse_map_shader_create_info);
    diffuse_map_pipeline
        = create_compute_pipeline(dev, pipeline_layout.get(), diffuse_map_shader_module.get());

    sampler = dev.createSamplerUnique(vk::SamplerCreateInfo{
        {}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear
    });
}
