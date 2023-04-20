#include "egg/renderer/scene_renderer.h"
#include <iostream>

struct vertex {
    float position[3], normals[3], tangents[3], tex_coords[2];
};

using index_type = uint32_t;

scene_renderer::scene_renderer(flecs::world& world, std::unique_ptr<render_pipeline> pipeline) {}

void scene_renderer::load_bundle(
    renderer* r, std::shared_ptr<asset_bundle> bundle, gpu_buffer&& staged_data
) {
    vk::UniqueCommandBuffer upload_cmds
        = std::move(r->dev->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{
            r->command_pool.get(), vk::CommandBufferLevel::ePrimary, 1})[0]);

    std::cout << "cmd buffer id = " << std::hex << upload_cmds.get() << std::dec << "\n"
              << "staging buffer id = " << staged_data.get() << "\n";

    upload_cmds->begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    const auto& bh = bundle->bundle_header();

    auto vertex_size = bh.num_total_vertices * sizeof(vertex);
    vertex_buffer    = std::make_unique<gpu_buffer>(
        r->allocator,
        vk::BufferCreateInfo{
               {},
            vertex_size,
            vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst},
        VmaAllocationCreateInfo{
               .usage = VMA_MEMORY_USAGE_AUTO,
        }
    );
    upload_cmds->copyBuffer(
        staged_data.get(),
        vertex_buffer->get(),
        vk::BufferCopy{bh.vertex_start_offset - bh.gpu_data_offset, 0, vertex_size}
    );

    auto index_size = bh.num_total_indices * sizeof(index_type);
    index_buffer    = std::make_unique<gpu_buffer>(
        r->allocator,
        vk::BufferCreateInfo{
               {},
            index_size,
            vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst},
        VmaAllocationCreateInfo{
               .usage = VMA_MEMORY_USAGE_AUTO,
        }
    );
    upload_cmds->copyBuffer(
        staged_data.get(),
        index_buffer->get(),
        vk::BufferCopy{bh.index_start_offset - bh.gpu_data_offset, 0, index_size}
    );

    auto subres_range = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
    std::vector<vk::ImageMemoryBarrier> undef_to_transfer_barriers,
        transfer_to_shader_read_barriers;
    for(texture_id i = 0; i < bundle->bundle_header().num_textures; ++i) {
        const auto& th = bundle->texture_by_index(i);
        std::cout << "texture " << bundle->string(th.name) << " " << th.id << "|" << th.name << "|"
                  << th.offset << " " << th.width << "x" << th.height << " "
                  << vk::to_string(vk::Format(th.format)) << "\n";
        /*auto props = r->phy_dev.getImageFormatProperties(
            vk::Format(th.format),
            vk::ImageType::e2D,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
            {}
        );
        std::cout << vk::to_string(vk::Format(th.format)) << " " << props.maxExtent.width << "x"
                  << props.maxExtent.height << " " << props.maxMipLevels << " max mips"
                  << " " << props.maxArrayLayers << " max layers"
                  << " " << props.maxResourceSize << " max size"
                  << " " << vk::to_string(props.sampleCounts) << " sample counts\n";*/
        auto img = std::make_unique<gpu_image>(
            r->allocator,
            vk::ImageCreateInfo{
                {},
                vk::ImageType::e2D,
                vk::Format(th.format),
                vk::Extent3D{th.width, th.height, 1},
                1,
                1,
                vk::SampleCountFlagBits::e1,
                vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled
        },
            VmaAllocationCreateInfo{.usage = VMA_MEMORY_USAGE_AUTO}
        );
        undef_to_transfer_barriers.emplace_back(
            vk::AccessFlags(),
            vk::AccessFlagBits::eTransferWrite,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            img->get(),
            subres_range
        );
        transfer_to_shader_read_barriers.emplace_back(
            vk::AccessFlagBits::eTransferWrite,
            vk::AccessFlagBits::eShaderRead,
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            img->get(),
            subres_range
        );
        textures.emplace(th.id, texture{std::move(img)});
    }

    upload_cmds->pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer,
        {},
        {},
        {},
        undef_to_transfer_barriers
    );
    for(texture_id i = 0; i < bundle->bundle_header().num_textures; ++i) {
        const auto& th = bundle->texture_by_index(i);
        upload_cmds->copyBufferToImage(
            staged_data.get(),
            textures.at(th.id).img->get(),
            vk::ImageLayout::eTransferDstOptimal,
            vk::BufferImageCopy{
                th.offset - bh.gpu_data_offset,
                0,
                0,
                vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                vk::Offset3D{0, 0, 0},
                vk::Extent3D{th.width, th.height, 1},
        }
        );
    }
    upload_cmds->pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eFragmentShader,
        {},
        {},
        {},
        transfer_to_shader_read_barriers
    );

    upload_cmds->end();
    auto fence = r->dev->createFenceUnique(vk::FenceCreateInfo{});
    // TODO: find somewhere else for this to go, that is more efficient
    r->graphics_queue.submit(
        vk::SubmitInfo{0, nullptr, nullptr, 1, &upload_cmds.get()}, fence.get()
    );
    std::cout << "waiting for upload...\n";
    auto err = r->dev->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
    std::cout << "upload complete!\n";
}

void scene_renderer::render_frame(frame& frame) {
    // frame.frame_cmd_buf.beginRenderPass({}, vk::SubpassContents::eSecondaryCommandBuffers);
    // frame.frame_cmd_buf.endRenderPass();
}
