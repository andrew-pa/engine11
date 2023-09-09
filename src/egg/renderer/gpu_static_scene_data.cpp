#include "egg/renderer/scene_renderer.h"
#include "egg/renderer/imgui_renderer.h"
#include <vulkan/vulkan_format_traits.hpp>

gpu_static_scene_data::gpu_static_scene_data(renderer* r, std::shared_ptr<asset_bundle> bundle, vk::CommandBuffer upload_cmds) {
    staging_buffer = std::make_unique<gpu_buffer>(
        r->gpu_alloc(),
        vk::BufferCreateInfo{{}, bundle->gpu_data_size(), vk::BufferUsageFlagBits::eTransferSrc},
        VmaAllocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                     | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO}
    );

    bundle->take_gpu_data((uint8_t*)staging_buffer->cpu_mapped());

    load_geometry_from_bundle(r->gpu_alloc(), bundle.get(), upload_cmds);
    create_textures_from_bundle(r, bundle.get());
    generate_upload_commands_for_textures(bundle.get(), upload_cmds);
    create_envs_from_bundle(r, bundle.get());
    generate_upload_commands_for_envs(bundle.get(), upload_cmds);

    r->imgui()->add_window("Static Resources", [this, bundle](bool* open) {
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

vk::ImageCreateInfo create_info_for_image(const asset_bundle_format::image& img, vk::ImageUsageFlags usage, vk::ImageCreateFlags flags = {}) {
    return vk::ImageCreateInfo{
        flags,
        vk::ImageType::e2D,
        vk::Format(img.format),
        vk::Extent3D{img.width, img.height, 1},
        img.mip_levels,
        img.array_layers,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        usage
    };
}

void gpu_static_scene_data::create_textures_from_bundle(renderer* r, asset_bundle* current_bundle) {
    for(texture_id i = 0; i < current_bundle->bundle_header().num_textures; ++i) {
        const auto& th = current_bundle->texture_by_index(i);
        auto img = std::make_unique<gpu_image>(
            r->gpu_alloc(),
            create_info_for_image(th.img, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled),
            VmaAllocationCreateInfo{.usage = VMA_MEMORY_USAGE_AUTO}
        );

        auto subres_range = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, th.img.mip_levels, 0, th.img.array_layers);

        auto img_view = r->device().createImageViewUnique(vk::ImageViewCreateInfo{
            {},
            img->get(),
            vk::ImageViewType::e2D,
            vk::Format(th.img.format),
            vk::ComponentMapping{},
            subres_range});

        auto imgui_id = r->imgui()->add_texture(img_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal);

        textures.emplace(th.id, texture{std::move(img), std::move(img_view), imgui_id});
    }
}

std::vector<vk::BufferImageCopy> make_copy_regions_for_image(
        asset_bundle* current_bundle,
        size_t offset,
        const asset_bundle_format::image& img)
{
    // copy each mip level from the buffer
    std::vector<vk::BufferImageCopy> regions;
    regions.reserve(img.mip_levels * img.array_layers);
    size_t internal_offset = 0;
    for(uint32_t layer_index = 0; layer_index < img.array_layers; ++layer_index) {
        uint32_t w = img.width, h = img.height;
        for(uint32_t mip_level = 0; mip_level < img.mip_levels; ++mip_level) {
            std::cout << "L" << layer_index << " M" << mip_level << " " << std::hex << internal_offset << std::dec << "\n";
            regions.emplace_back(
                    vk::BufferImageCopy{
                    offset - current_bundle->bundle_header().gpu_data_offset + internal_offset,
                    0, 0,
                    vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, mip_level, layer_index, 1},
                    vk::Offset3D{0, 0, 0},
                    vk::Extent3D{w, h, 1},
                    }
                    );
            internal_offset += w * h * vk::blockSize(vk::Format(img.format));
            w = glm::max(w/2, 1u); h = glm::max(h/2, 1u);
        }
    }
    return regions;
}

void gpu_static_scene_data::generate_upload_commands_for_textures(asset_bundle* current_bundle, vk::CommandBuffer upload_cmds) {
    auto subres_range = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, VK_REMAINING_MIP_LEVELS, 0, 1);
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

        std::vector<vk::BufferImageCopy> regions = make_copy_regions_for_image(current_bundle, th.offset, th.img);

        upload_cmds.copyBufferToImage(
                staging_buffer->get(),
                textures.at(th.id).img->get(),
                vk::ImageLayout::eTransferDstOptimal,
                regions);
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

void gpu_static_scene_data::create_envs_from_bundle(renderer* r, asset_bundle* current_bundle) {
    for(size_t i = 0; i < current_bundle->bundle_header().num_environments; ++i) {
        const auto& ev = current_bundle->environment_by_index(i);
        auto img = std::make_unique<gpu_image>(
            r->gpu_alloc(),
            create_info_for_image(
                ev.skybox,
                vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                vk::ImageCreateFlagBits::eCubeCompatible),
            VmaAllocationCreateInfo{.usage = VMA_MEMORY_USAGE_AUTO}
        );

        auto subres_range = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, ev.skybox.mip_levels, 0, 1);

        auto img_view = r->device().createImageViewUnique(vk::ImageViewCreateInfo{
            {},
            img->get(),
            vk::ImageViewType::e2D,
            vk::Format(ev.skybox.format),
            vk::ComponentMapping{},
            subres_range
        });

        subres_range.setLayerCount(6);
        auto sky_cube_view = r->device().createImageViewUnique(vk::ImageViewCreateInfo{
            {},
            img->get(),
            vk::ImageViewType::eCube,
            vk::Format(ev.skybox.format),
            vk::ComponentMapping{},
            subres_range
        });

        auto imgui_id = r->imgui()->add_texture(img_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal);

        envs.emplace(ev.name, environment{
            .sky_img = std::move(img),
            .sky_preview_img_view = std::move(img_view),
            .skybox_img_view = std::move(sky_cube_view),
            .imgui_id = imgui_id
        });
        current_env = ev.name;
    }
}

void gpu_static_scene_data::generate_upload_commands_for_envs(asset_bundle* current_bundle, vk::CommandBuffer upload_cmds) {
    auto subres_range = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor,
            0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS);
    std::vector<vk::ImageMemoryBarrier> undef_to_transfer_barriers,
        transfer_to_shader_read_barriers;
    for(const auto& [_, ev] : envs) {
        undef_to_transfer_barriers.emplace_back(
            vk::AccessFlags(),
            vk::AccessFlagBits::eTransferWrite,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            ev.sky_img->get(),
            subres_range
        );
        transfer_to_shader_read_barriers.emplace_back(
            vk::AccessFlagBits::eTransferWrite,
            vk::AccessFlagBits::eShaderRead,
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            ev.sky_img->get(),
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

    for(size_t i = 0; i < current_bundle->bundle_header().num_environments; ++i) {
        const auto& ev = current_bundle->environment_by_index(i);

        auto regions = make_copy_regions_for_image(current_bundle, ev.skybox_offset, ev.skybox);

        upload_cmds.copyBufferToImage(
                staging_buffer->get(),
                envs.at(ev.name).sky_img->get(),
                vk::ImageLayout::eTransferDstOptimal,
                regions);
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
        // environment skybox
        {4, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}
    };

    desc_set_layout
        = r->device().createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo{
            {}, sizeof(bindings) / sizeof(bindings[0]), bindings});

    vk::DescriptorPoolSize pool_sizes[] = {
        {vk::DescriptorType::eStorageBuffer,        2          },
        {vk::DescriptorType::eUniformBuffer,        1          },
        {vk::DescriptorType::eCombinedImageSampler,
         (uint32_t)current_bundle->bundle_header().num_textures + 1},
    };
    desc_set_pool = r->device().createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo{
        {}, 1, sizeof(pool_sizes) / sizeof(pool_sizes[0]), pool_sizes});

    desc_set = r->device().allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
        desc_set_pool.get(), desc_set_layout.get()})[0];

    std::vector<vk::DescriptorImageInfo> texture_infos;
    texture_infos.reserve(current_bundle->bundle_header().num_textures + 1);
    size_t i;
    for(i = 0; i < current_bundle->bundle_header().num_textures; ++i) {
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

    // environment map skybox
    writes.emplace_back(
        desc_set,
        4,
        0,
        1,
        vk::DescriptorType::eCombinedImageSampler,
        texture_infos.data() + i
    );
    texture_infos.emplace_back(
        texture_sampler.get(), envs.at(current_env).skybox_img_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal
    );

    return texture_infos;
}

void gpu_static_scene_data::resource_upload_cleanup() {
    staging_buffer.reset();
}

void gpu_static_scene_data::texture_window_gui(bool* open, std::shared_ptr<asset_bundle> current_bundle) {
    ImGui::Begin("Static Resources", open);
    if(ImGui::BeginTabBar("##static-resources")) {
        if(ImGui::BeginTabItem("Textures")) {
            if(ImGui::BeginTable("#TextureTable", 4, ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Format");
                ImGui::TableSetupColumn("Size");
                ImGui::TableSetupColumn("Preview");
                ImGui::TableHeadersRow();
                for(const auto& [id, tx] : textures) {
                    const auto& th = current_bundle->texture(id);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    // TODO: when ImStrv lands we won't need this copy
                    auto name = std::string(current_bundle->string(th.name));
                    ImGui::Text("%s", name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", vk::to_string(vk::Format(th.img.format)).c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%u x %u (%u mips, %u layers)", th.img.width, th.img.height, th.img.mip_levels, th.img.array_layers);
                    ImGui::TableNextColumn();
                    ImGui::Image((ImTextureID)tx.imgui_id, ImVec2(256, 256));
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }

        if(ImGui::BeginTabItem("Materials")) {
            if(ImGui::BeginTable("#MaterialTable", 6, ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Texture IDs");
                ImGui::TableSetupColumn("Base Color");
                ImGui::TableSetupColumn("Normals");
                ImGui::TableSetupColumn("Roughness");
                ImGui::TableSetupColumn("Metallic");
                ImGui::TableHeadersRow();
                for(size_t i = 0; i < current_bundle->bundle_header().num_materials; ++i) {
                    const auto& m = current_bundle->material(i);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    // TODO: when ImStrv lands we won't need this copy
                    auto name = std::string(current_bundle->string(m.name));
                    ImGui::Text("%s", name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("B%u N%u R%u M%u", m.base_color, m.normals, m.roughness, m.metallic);
                    for(texture_id id : {m.base_color, m.normals, m.roughness, m.metallic}) {
                        ImGui::TableNextColumn();
                        auto bc = textures.find(id);
                        if(bc != textures.end())
                            ImGui::Image((ImTextureID)bc->second.imgui_id, ImVec2(64, 64));
                        else
                            ImGui::Text("-");
                    }
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }

        if(ImGui::BeginTabItem("Geometry")) {
            for(size_t gi = 0; gi < current_bundle->bundle_header().num_groups; ++gi) {
                auto name = std::string(current_bundle->string(current_bundle->group_name(gi)));
                ImGui::PushID(gi);
                if(ImGui::TreeNodeEx("#",
                            ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick,
                            "%lu %s", gi, name.c_str())) {
                    for(auto ob = current_bundle->group_objects(gi); ob.has_more(); ++ob) {
                        auto ob_id = *ob;
                        auto name = std::string(current_bundle->string(current_bundle->object_name(ob_id)));
                        if(ImGui::TreeNode((void*)ob_id, "%u %s", ob_id, name.c_str())) {
                            size_t id = 0;
                            for(auto m = current_bundle->object_meshes(ob_id); m.has_more(); ++m) {
                                if(ImGui::TreeNodeEx((void*)id,
                                            ImGuiTreeNodeFlags_Leaf,
                                            "mesh [V@%lx I@%lx:%lx M%lu]",
                                            m->vertex_offset, m->index_offset, m->index_count, m->material_index))
                                    ImGui::TreePop();
                                id++;
                            }
                            ImGui::TreePop();
                        }
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }

        if(ImGui::BeginTabItem("Environments")) {
            if(ImGui::BeginTable("#env-table", 2)) {
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Skybox");
                ImGui::TableHeadersRow();
                for(const auto&[name, ev] : envs) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    auto name_s = std::string(current_bundle->string(name));
                    ImGui::Text("%s", name_s.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Image((ImTextureID)ev.imgui_id, ImVec2(256, 256));
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::End();
}
