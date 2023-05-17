#include "egg/renderer/scene_renderer.h"
#include "egg/renderer/imgui_renderer.h"

gpu_static_scene_data::gpu_static_scene_data(renderer* r, std::shared_ptr<asset_bundle> bundle, vk::CommandBuffer upload_cmds) {
    staging_buffer = std::make_unique<gpu_buffer>(
        r->gpu_allocator(),
        vk::BufferCreateInfo{{}, bundle->gpu_data_size(), vk::BufferUsageFlagBits::eTransferSrc},
        VmaAllocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                     | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO}
    );

    bundle->take_gpu_data((uint8_t*)staging_buffer->cpu_mapped());

    load_geometry_from_bundle(r->gpu_allocator(), bundle.get(), upload_cmds);
    create_textures_from_bundle(r, bundle.get());
    generate_upload_commands_for_textures(bundle.get(), upload_cmds);

    r->imgui()->add_window("Textures", [&](bool* open) {
        this->texture_window_gui(open, bundle);
    });
}

void gpu_static_scene_data::load_geometry_from_bundle(
    std::shared_ptr<gpu_allocator> allocator,
    asset_bundle* current_bundle,
    vk::CommandBuffer upload_cmds
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

void gpu_static_scene_data::create_textures_from_bundle(renderer* r, asset_bundle* current_bundle) {
    auto subres_range = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
    for(texture_id i = 0; i < current_bundle->bundle_header().num_textures; ++i) {
        const auto& th = current_bundle->texture_by_index(i);
        auto img = std::make_unique<gpu_image>(
            r->gpu_allocator(),
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

        auto img_view = r->device().createImageViewUnique(vk::ImageViewCreateInfo{
            {},
            img->get(),
            vk::ImageViewType::e2D,
            vk::Format(th.format),
            vk::ComponentMapping{},
            subres_range});

        auto imgui_id = r->imgui()->add_texture(img_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal);

        textures.emplace(th.id, texture{std::move(img), std::move(img_view), imgui_id});
    }
}

void gpu_static_scene_data::generate_upload_commands_for_textures(asset_bundle* current_bundle, vk::CommandBuffer upload_cmds) {
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

std::vector<vk::DescriptorImageInfo> gpu_static_scene_data::setup_descriptors(renderer* r, asset_bundle* current_bundle, std::vector<vk::WriteDescriptorSet>& writes) {
    texture_sampler = r->device().createSamplerUnique(vk::SamplerCreateInfo{
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

    // TODO: we should create the descriptor set layout in the scene_renderer?
    vk::DescriptorSetLayoutBinding bindings[] = {
		// transforms storage buffer
        {0, vk::DescriptorType::eStorageBuffer,     1,           vk::ShaderStageFlagBits::eAll},
		// scene textures
        {1,
         vk::DescriptorType::eCombinedImageSampler,
         (uint32_t)current_bundle->bundle_header().num_textures,
         vk::ShaderStageFlagBits::eAll                                                        },
        // per-frame shader uniforms
        {2, vk::DescriptorType::eUniformBuffer,     1,           vk::ShaderStageFlagBits::eAll},
		// lights storage buffer
        {3, vk::DescriptorType::eStorageBuffer,     1,           vk::ShaderStageFlagBits::eAll},
    };

    desc_set_layout
        = r->device().createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo{
            {}, sizeof(bindings) / sizeof(bindings[0]), bindings});

    vk::DescriptorPoolSize pool_sizes[] = {
        {vk::DescriptorType::eStorageBuffer,        2          },
        {vk::DescriptorType::eUniformBuffer,        1          },
        {vk::DescriptorType::eCombinedImageSampler,
         (uint32_t)current_bundle->bundle_header().num_textures},
    };
    desc_set_pool = r->device().createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo{
        {}, 1, sizeof(pool_sizes) / sizeof(pool_sizes[0]), pool_sizes});

    desc_set = r->device().allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
        desc_set_pool.get(), desc_set_layout.get()})[0];

    std::vector<vk::DescriptorImageInfo> texture_infos;
    texture_infos.reserve(current_bundle->bundle_header().num_textures);
    for(size_t i = 0; i < current_bundle->bundle_header().num_textures; ++i) {
        // TODO: again the assumption is that texture IDs are contiguous from 1
        const auto& tx = textures.at(i + 1);
        writes.emplace_back(
            desc_set,
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

    return texture_infos;
}

void gpu_static_scene_data::resource_upload_cleanup() {
    staging_buffer.reset();
}

void gpu_static_scene_data::texture_window_gui(bool* open, std::shared_ptr<asset_bundle> current_bundle) {
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
