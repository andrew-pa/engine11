#include "asset-bundler/texture_process_jobs.h"
#include <vulkan/vulkan_format_traits.hpp>

texture_process_job::texture_process_job(vk::Device dev, vk::CommandPool cmd_pool, const std::shared_ptr<gpu_allocator>& alloc, texture_info* info)
    : process_job(dev, cmd_pool), image_info{
         info->img.vulkan_create_info(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst)
    }
{
    total_output_size = linear_image_size_in_bytes(image_info);

    img = std::make_unique<gpu_image>(alloc, image_info);
    init_staging_buffer(alloc, total_output_size);

    // copy original data into staging buffer
    memcpy(staging->cpu_mapped(), info->data, info->img.width*info->img.height*vk::blockSize(info->img.format));

    cmd_buffer->begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    // transition the first mip level to transfer destination to prepare to recieve staging buffer data
    cmd_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
            {}, {}, {}, {
            vk::ImageMemoryBarrier {
                vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eTransferWrite,
                vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                img->get(),
                vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor,0,1,0,1}
            }
        });

    // copy original image data from staging buffer to first mip level
    cmd_buffer->copyBufferToImage(staging->get(), img->get(),
            vk::ImageLayout::eTransferDstOptimal,
            vk::BufferImageCopy {
                0, 0, 0,
                vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                vk::Offset3D{0,0,0},
                vk::Extent3D{info->img.width, info->img.height,1}
            });
}

void texture_process_job::generate_mipmaps() {
    // create structures for submitting commands
    vk::ImageBlit blit_info {
        vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        {vk::Offset3D{0,0,0},{(int32_t)image_info.extent.width,(int32_t)image_info.extent.height,1}},
        vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 1, 0, 1},
        {vk::Offset3D{0,0,0},{glm::max((int32_t)image_info.extent.width/2, 1),
            glm::max((int32_t)image_info.extent.height/2, 1),1}},
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
    cmd_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
            {}, {}, {}, {
            barrierUninitToDst,
            barrierDstToSrc
        });

    // generate each mip level by copying from the last one
    for(uint32_t i = 1; i < image_info.mipLevels; ++i) {
        cmd_buffer->blitImage(
                img->get(), vk::ImageLayout::eTransferSrcOptimal,
                img->get(), vk::ImageLayout::eTransferDstOptimal,
                1, &blit_info,
                vk::Filter::eLinear);

        if(i + 1 < image_info.mipLevels) {
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

            cmd_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
                    {}, {}, {}, {barrierUninitToDst, barrierDstToSrc});
        }
    }

    // transition the last mip level from transfer destination to transfer source
    barrierDstToSrc.subresourceRange.setBaseMipLevel(image_info.mipLevels - 1);
    cmd_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
            {}, {}, {}, { barrierDstToSrc });

    size_t offset = 0;
    auto regions = copy_regions_for_linear_image2d(
            image_info.extent.width, image_info.extent.height,
            image_info.mipLevels, image_info.arrayLayers, image_info.format, offset);
    cmd_buffer->copyImageToBuffer(
            img->get(),
            vk::ImageLayout::eTransferSrcOptimal,
            staging->get(),
            regions);
}
