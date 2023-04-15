#include "egg/renderer/scene_renderer.h"

struct vertex {};

using index_type = uint32_t;

scene_renderer::scene_renderer(flecs::world& world, std::unique_ptr<render_pipeline> pipeline) {}

void scene_renderer::load_bundle(renderer* r, std::shared_ptr<asset_bundle> bundle, gpu_buffer&& staged_data) {
    vk::CommandBuffer upload_cmds;

    const auto& bh = bundle->bundle_header();

    auto vertex_size = bh.num_total_vertices * sizeof(vertex);
    vertex_buffer    = std::make_unique<gpu_buffer>(
        r->allocator,
        vk::BufferCreateInfo{{}, vertex_size, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst},
        VmaAllocationCreateInfo{
               .usage = VMA_MEMORY_USAGE_AUTO,
        }
    );
    auto index_size = bh.num_total_indices * sizeof(index_type);
    index_buffer    = std::make_unique<gpu_buffer>(
        r->allocator,
        vk::BufferCreateInfo{{}, index_size, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst},
        VmaAllocationCreateInfo{
               .usage = VMA_MEMORY_USAGE_AUTO,
        }
    );

    upload_cmds.copyBuffer(
        staged_data.get(),
        vertex_buffer->get(),
        {
            vk::BufferCopy{bh.vertex_start_offset - bh.gpu_data_offset, 0, vertex_size}
    }
    );
    upload_cmds.copyBuffer(
        staged_data.get(),
        index_buffer->get(),
        {
            vk::BufferCopy{bh.index_start_offset - bh.gpu_data_offset, 0, index_size}
    }
    );

    for(texture_id tid = 1; tid <= bundle->bundle_header().num_textures; ++tid)
        const auto& th = bundle->texture(tid);

    upload_cmds.end();
    r->graphics_queue.submit(
        {
            vk::SubmitInfo{0, nullptr, nullptr, 1, &upload_cmds}
    },
        nullptr
    );
    r->graphics_queue.waitIdle();
}

void scene_renderer::render_frame(frame& frame) {
    // frame.frame_cmd_buf.beginRenderPass({}, vk::SubpassContents::eSecondaryCommandBuffers);
    // frame.frame_cmd_buf.endRenderPass();
}
