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

    for(texture_id i = 0; i < bundle->bundle_header().num_textures; ++i) {
        const auto& th = bundle->texture_by_index(i);
        std::cout << "texture " << bundle->string(th.name) << " " << th.id << "|" << th.name << "|"
                  << th.offset << " " << th.width << "x" << th.height << " "
                  << vk::to_string(vk::Format(th.format)) << "\n";
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
        upload_cmds->copyBufferToImage(
            staged_data.get(),
            img->get(),
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::BufferImageCopy{
                th.offset - bh.gpu_data_offset,
                0,
                0,
                vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                vk::Offset3D{0, 0, 0},
                vk::Extent3D{th.width, th.height, 1},
        }
        );
        textures.emplace(th.id, texture{std::move(img)});
    }

    upload_cmds->end();
    r->graphics_queue.submit(vk::SubmitInfo{0, nullptr, nullptr, 1, &upload_cmds.get()}, nullptr);
    r->graphics_queue.waitIdle(
    );  // TODO: find somewhere else for this to go, that is more efficient
}

void scene_renderer::render_frame(frame& frame) {
    // frame.frame_cmd_buf.beginRenderPass({}, vk::SubpassContents::eSecondaryCommandBuffers);
    // frame.frame_cmd_buf.endRenderPass();
}
