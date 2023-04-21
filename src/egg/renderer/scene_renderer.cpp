#include "egg/renderer/scene_renderer.h"
#include "egg/renderer/imgui_renderer.h"
#include <glm/glm.hpp>
#include <iostream>

/* TODO:
 * transforms uniform buffer & allocator +
 * descriptor sets (layouts, sets) +
 * render pipeline
 * handle scene updates
 * ****/

using glm::vec2;
using glm::vec3;

struct vertex {
    vec3 position, normal, tangent;
    vec2 tex_coords;
};

using index_type = uint32_t;

scene_renderer::scene_renderer(
    renderer* r, flecs::world& world, std::unique_ptr<rendering_algorithm> algo
)
    : transforms(r->allocator, 384, vk::BufferUsageFlagBits::eStorageBuffer) {
    r->ir->add_window("Textures", [&](bool* open) { this->texture_window_gui(open); });
}

void scene_renderer::start_resource_upload(
    renderer* r, std::shared_ptr<asset_bundle> bundle, vk::CommandBuffer upload_cmds
) {
    current_bundle = bundle;

    staging_buffer = std::make_unique<gpu_buffer>(
        r->allocator,
        vk::BufferCreateInfo{{}, bundle->gpu_data_size(), vk::BufferUsageFlagBits::eTransferSrc},
        VmaAllocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                     | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO}
    );

    bundle->take_gpu_data((byte*)staging_buffer->cpu_mapped());

    load_geometry_from_bundle(r->allocator, upload_cmds);
    create_textures_from_bundle(r);
    generate_upload_commands_for_textures(upload_cmds);
}

void scene_renderer::load_geometry_from_bundle(
    VmaAllocator allocator, vk::CommandBuffer upload_cmds
) {
    const auto& bh          = current_bundle->bundle_header();
    auto        vertex_size = bh.num_total_vertices * sizeof(vertex);
    vertex_buffer           = std::make_unique<gpu_buffer>(
        allocator,
        vk::BufferCreateInfo{
                      {},
            vertex_size,
            vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst},
        VmaAllocationCreateInfo{
                      .usage = VMA_MEMORY_USAGE_AUTO,
        }
    );
    upload_cmds.copyBuffer(
        staging_buffer->get(),
        vertex_buffer->get(),
        vk::BufferCopy{bh.vertex_start_offset - bh.gpu_data_offset, 0, vertex_size}
    );

    auto index_size = bh.num_total_indices * sizeof(index_type);
    index_buffer    = std::make_unique<gpu_buffer>(
        allocator,
        vk::BufferCreateInfo{
               {},
            index_size,
            vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst},
        VmaAllocationCreateInfo{
               .usage = VMA_MEMORY_USAGE_AUTO,
        }
    );
    upload_cmds.copyBuffer(
        staging_buffer->get(),
        index_buffer->get(),
        vk::BufferCopy{bh.index_start_offset - bh.gpu_data_offset, 0, index_size}
    );
}

void scene_renderer::create_textures_from_bundle(renderer* r) {
    auto subres_range = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
    for(texture_id i = 0; i < current_bundle->bundle_header().num_textures; ++i) {
        const auto& th = current_bundle->texture_by_index(i);
        std::cout << "texture " << current_bundle->string(th.name) << " " << th.id << "|" << th.name
                  << "|" << th.offset << " " << th.width << "x" << th.height << " "
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

        auto img_view = r->dev->createImageViewUnique(vk::ImageViewCreateInfo{
            {},
            img->get(),
            vk::ImageViewType::e2D,
            vk::Format(th.format),
            vk::ComponentMapping{},
            subres_range});

        auto imgui_id = r->ir->add_texture(img_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal);

        textures.emplace(th.id, texture{std::move(img), std::move(img_view), imgui_id});
    }
}

void scene_renderer::generate_upload_commands_for_textures(vk::CommandBuffer upload_cmds) {
    auto subres_range = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
    std::vector<vk::ImageMemoryBarrier> undef_to_transfer_barriers,
        transfer_to_shader_read_barriers;
    for(const auto& [id, tx] : textures) {
        undef_to_transfer_barriers.emplace_back(
            vk::AccessFlags(),
            vk::AccessFlagBits::eTransferWrite,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            tx.img->get(),
            subres_range
        );
        transfer_to_shader_read_barriers.emplace_back(
            vk::AccessFlagBits::eTransferWrite,
            vk::AccessFlagBits::eShaderRead,
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            tx.img->get(),
            subres_range
        );
    }

    upload_cmds.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer,
        {},
        {},
        {},
        undef_to_transfer_barriers
    );

    for(texture_id i = 0; i < current_bundle->bundle_header().num_textures; ++i) {
        const auto& th = current_bundle->texture_by_index(i);
        upload_cmds.copyBufferToImage(
            staging_buffer->get(),
            textures.at(th.id).img->get(),
            vk::ImageLayout::eTransferDstOptimal,
            vk::BufferImageCopy{
                th.offset - current_bundle->bundle_header().gpu_data_offset,
                0,
                0,
                vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                vk::Offset3D{0, 0, 0},
                vk::Extent3D{th.width, th.height, 1},
        }
        );
    }

    upload_cmds.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eFragmentShader,
        {},
        {},
        {},
        transfer_to_shader_read_barriers
    );
}

void scene_renderer::setup_scene_post_upload(renderer* r) {
    texture_sampler = r->dev->createSamplerUnique(vk::SamplerCreateInfo{
        {},
        vk::Filter::eLinear,
        vk::Filter::eLinear,
        vk::SamplerMipmapMode::eLinear,
        vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode::eRepeat,
        0.f,
        VK_FALSE,
        16.f});

    vk::DescriptorSetLayoutBinding bindings[] = {
  // transforms storage buffer
        {0, vk::DescriptorType::eStorageBuffer,     1,           vk::ShaderStageFlagBits::eAll},
 // scene textures
        {1,
         vk::DescriptorType::eCombinedImageSampler,
         (uint32_t)current_bundle->bundle_header().num_textures,
         vk::ShaderStageFlagBits::eAll                                                        }
    };

    scene_data_desc_set_layout
        = r->dev->createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo{
            {}, sizeof(bindings) / sizeof(bindings[0]), bindings});

    vk::DescriptorPoolSize pool_sizes[] = {
        {vk::DescriptorType::eStorageBuffer,        1          },
        {vk::DescriptorType::eCombinedImageSampler,
         (uint32_t)current_bundle->bundle_header().num_textures},
    };
    scene_data_desc_set_pool = r->dev->createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo{
        {}, 1, sizeof(pool_sizes) / sizeof(pool_sizes[0]), pool_sizes});

    scene_data_desc_set = r->dev->allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
        scene_data_desc_set_pool.get(), scene_data_desc_set_layout.get()})[0];

    std::vector<vk::WriteDescriptorSet> writes;

    vk::DescriptorBufferInfo transforms_buffer_info{transforms.get(), 0, VK_WHOLE_SIZE};
    writes.emplace_back(
        scene_data_desc_set,
        0,
        0,
        1,
        vk::DescriptorType::eStorageBuffer,
        nullptr,
        &transforms_buffer_info
    );

    std::vector<vk::DescriptorImageInfo> texture_infos;
    texture_infos.reserve(current_bundle->bundle_header().num_textures);
    for(size_t i = 0; i < current_bundle->bundle_header().num_textures; ++i) {
        // TODO: again the assumption is that texture IDs are contiguous from 1
        const auto& tx = textures.at(i + 1);
        writes.emplace_back(
            scene_data_desc_set,
            1,
            i,
            1,
            vk::DescriptorType::eCombinedImageSampler,
            texture_infos.data() + i
        );
        texture_infos.emplace_back(
            texture_sampler.get(), tx.img_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal
        );
    }

    r->dev->updateDescriptorSets(writes.size(), writes.data(), 0, nullptr);
}

void scene_renderer::resource_upload_cleanup() { staging_buffer.reset(); }

void scene_renderer::render_frame(frame& frame) {
    // frame.frame_cmd_buf.beginRenderPass({}, vk::SubpassContents::eSecondaryCommandBuffers);
    // frame.frame_cmd_buf.endRenderPass();
}

void scene_renderer::texture_window_gui(bool* open) {
    ImGui::Begin("Textures", open);
    if(ImGui::BeginTable("#TextureTable", 4, ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Format");
        ImGui::TableSetupColumn("Size");
        ImGui::TableSetupColumn("Preview");
        ImGui::TableHeadersRow();
        for(auto& [id, tx] : textures) {
            const auto& th = current_bundle->texture(id);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            // TODO: when ImStrv lands we won't need this copy
            auto name = std::string(current_bundle->string(th.name));
            ImGui::Text("%s", name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", vk::to_string(vk::Format(th.format)).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%u x %u", th.width, th.height);
            ImGui::TableNextColumn();
            ImGui::Image((ImTextureID)tx.imgui_id, ImVec2(256, 256));
        }
        ImGui::EndTable();
    }
    ImGui::End();
}
