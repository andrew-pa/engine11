#include "egg/renderer/scene_renderer.h"
#include "egg/components.h"
#include "egg/renderer/imgui_renderer.h"
#include <iostream>
#include <unordered_set>
#include <utility>

/* TODO:
 * transforms uniform buffer & allocator +
 * descriptor sets (layouts, sets) +
 * render pipeline
 *   lifecycle +
 *   actual draw calls +
 *   implementation +
 * handle scene updates +
 *   cameras +
 *   objects +
 * hot loading
 *   shaders
 *   render algorithms
 * ImGui remove/reregister window
 * performance metrics/graphs
 *   load times
 *   cpu/gpu frame times per subsystem (render, etc)
 * do we need the renderer/core directory? should memory.h be in there?
 * move swap_chain.cpp -> frame_renderer.cpp
 * move device.cpp -> renderer.cpp
 * move static resources into seperate class
 * move ECS setup into seperate scene_renderer function
 * ****/

// future cool things:
// use compute shaders for mesh skinning, marching cubes for metaballs -> mercury!
//  could find a compute queue that is preferentially on a different GPU for parallel compute,
//  downside is GPU-to-GPU copy

using glm::vec2;
using glm::vec3;

scene_renderer::scene_renderer(
    renderer* r, std::shared_ptr<flecs::world> _world, std::unique_ptr<rendering_algorithm> _algo
)
    : r(r), world(std::move(_world)), algo(std::move(_algo)),
      transforms(r->allocator, 384, vk::BufferUsageFlagBits::eStorageBuffer),
      should_regenerate_command_buffer(true) {
    r->ir->add_window("Textures", [&](bool* open) { this->texture_window_gui(open); });

    std::unordered_set<vk::Format> supported_depth_formats {
        vk::Format::eD16Unorm, vk::Format::eD16UnormS8Uint, vk::Format::eD24UnormS8Uint, vk::Format::eX8D24UnormPack32, vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint
    };
    for(auto i = supported_depth_formats.begin(); i != supported_depth_formats.end();) {
        auto props = r->phy_dev.getFormatProperties(*i);
        if((props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) != vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
            i = supported_depth_formats.erase(i);
        } else {
            i++;
        }
    }

    algo->init_with_device(r->dev.get(), r->allocator, supported_depth_formats);

    auto buffers            = r->dev->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{
        r->command_pool.get(), vk::CommandBufferLevel::eSecondary, 1});
    scene_render_cmd_buffer = std::move(buffers[0]);

    world->observer<comp::position, comp::rotation>()
        .event(flecs::OnAdd)
        .event(flecs::OnRemove)
        .each([&](flecs::iter& it, size_t i, const comp::position& p, const comp::rotation& r) {
            if(it.event() == flecs::OnAdd) {
                std::cout << "add transform\n";
                auto [t, bi] = transforms.alloc();
                comp::gpu_transform gt{.transform = t, .gpu_index = bi};
                std::optional<mat4> s;
                const auto*         obj = it.entity(i).get<comp::renderable>();
                if(obj != nullptr) s = current_bundle->object_transform(obj->object);
                gt.update(p, r, s);
                if(it.entity(i).has<tag::active_camera>()) *gt.transform = inverse(*gt.transform);
                it.entity(i).set<comp::gpu_transform>(gt);
            } else if(it.event() == flecs::OnRemove) {
                transforms.free(it.entity(i).get<comp::gpu_transform>()->gpu_index);
                it.entity(i).remove<comp::gpu_transform>();
            }
        });
    world->observer<comp::position, comp::rotation, comp::gpu_transform>()
        .event(flecs::OnSet)
        .each([&](flecs::iter&          it,
                  size_t                i,
                  const comp::position& p,
                  const comp::rotation& r,
                  comp::gpu_transform&  t) {
            std::optional<mat4> s;
            const auto*         obj = it.entity(i).get<comp::renderable>();
            if(obj != nullptr) s = current_bundle->object_transform(obj->object);
            if(it.entity(i).has<tag::active_camera>()) *t.transform = inverse(*t.transform);
            t.update(p, r);
        });
    world->observer<comp::camera>()
        .event(flecs::OnAdd)
        .event(flecs::OnSet)
        .event(flecs::OnRemove)
        .each([&](flecs::iter& it, size_t i, comp::camera& cam) {
            if(it.event() == flecs::OnAdd) {
                std::cout << "add camera\n";
                cam.proj_transform = transforms.alloc();
                cam.update(r->fr->aspect_ratio());
            } else if(it.event() == flecs::OnSet) {
                std::cout << "set camera\n";
                cam.update(r->fr->aspect_ratio());
            } else if(it.event() == flecs::OnRemove) {
                transforms.free(cam.proj_transform.second);
            }
        });
    world->observer<comp::gpu_transform, comp::renderable>()
        .event(flecs::OnAdd)
        .event(flecs::OnSet)
        .event(flecs::OnRemove)
        .iter([&](flecs::iter&, comp::gpu_transform*, comp::renderable*) {
            should_regenerate_command_buffer = true;
        });
    active_camera_q = world->query<tag::active_camera, comp::gpu_transform, comp::camera>();
    renderable_q    = world->query<comp::gpu_transform, comp::renderable>();

    algo->create_static_objects(vk::AttachmentDescription{
        vk::AttachmentDescriptionFlags(),
        r->surface_format.format,
        vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::ePresentSrcKHR});
}

void scene_renderer::start_resource_upload(
    std::shared_ptr<asset_bundle> bundle, vk::CommandBuffer upload_cmds
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
    create_textures_from_bundle();
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

void scene_renderer::create_textures_from_bundle() {
    auto subres_range = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
    for(texture_id i = 0; i < current_bundle->bundle_header().num_textures; ++i) {
        const auto& th = current_bundle->texture_by_index(i);
        // std::cout << "texture " << current_bundle->string(th.name) << " " << th.id << "|" <<
        // th.name
        //           << "|" << th.offset << " " << th.width << "x" << th.height << " "
        //           << vk::to_string(vk::Format(th.format)) << "\n";
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

void scene_renderer::setup_scene_post_upload() {
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

    algo->create_pipeline_layouts(
        scene_data_desc_set_layout.get(),
        vk::PushConstantRange{
            vk::ShaderStageFlagBits::eAll,
            0,
            sizeof(uint32_t) * 2 + sizeof(per_object_push_constants)}
    );

    algo->create_pipelines();
}

void scene_renderer::resource_upload_cleanup() { staging_buffer.reset(); }

void scene_renderer::create_swapchain_depd(frame_renderer* fr) {
    algo->create_framebuffers(fr);

    // update active camera's perspective matrix
    active_camera_q.each([&](flecs::iter& it,
                             size_t       i,
                             tag::active_camera,
                             comp::gpu_transform& view_tf,
                             comp::camera&        cam) { cam.update(fr->aspect_ratio()); });

    should_regenerate_command_buffer = true;
}

void scene_renderer::generate_scene_draw_commands(vk::CommandBuffer cb, vk::PipelineLayout pl) {
    cb.bindVertexBuffers(0, vertex_buffer->get(), {0});
    cb.bindIndexBuffer(index_buffer->get(), 0, vk::IndexType::eUint32);
    active_camera_q.each([&](flecs::iter&,
                             size_t,
                             tag::active_camera,
                             const comp::gpu_transform& view_tf,
                             const comp::camera&        cam) {
        cb.pushConstants<uint32_t>(
            pl,
            vk::ShaderStageFlagBits::eAll,
            0,
            {(uint32_t)view_tf.gpu_index, (uint32_t)cam.proj_transform.second}
        );
    });
    renderable_q.each(
        [&](flecs::iter&, size_t i, const comp::gpu_transform& t, const comp::renderable& r) {
            for(auto mi = current_bundle->object_meshes(r.object); mi.has_more(); ++mi) {
                const auto& mat = current_bundle->material(mi->material_index);
                cb.pushConstants<per_object_push_constants>(
                    pl,
                    vk::ShaderStageFlagBits::eAll,
                    2 * sizeof(uint32_t),
                    {
                        {.transform_index = (uint32_t)t.gpu_index,
                         .base_color      = static_cast<texture_id>(mat.base_color - 1),
                         .normals         = static_cast<texture_id>(mat.normals - 1),
                         .roughness       = static_cast<texture_id>(mat.roughness - 1),
                         .metallic        = static_cast<texture_id>(mat.metallic - 1)}
                }
                );
                cb.drawIndexed(mi->index_count, 1, mi->index_offset, mi->vertex_offset, 0);
            }
        }
    );
}

void scene_renderer::render_frame(frame& frame) {
    // update scene data ie transforms, possibly mark command buffers for invalidation
    // if command buffers should be invalidated, reset and regenerate it, otherwise just submit it
    if(should_regenerate_command_buffer) {
        should_regenerate_command_buffer = false;

        auto cb = scene_render_cmd_buffer.get();
        cb.begin(vk::CommandBufferBeginInfo{
            vk::CommandBufferUsageFlagBits::eSimultaneousUse
                | vk::CommandBufferUsageFlagBits::eRenderPassContinue,
            algo->get_command_buffer_inheritance_info()});

        // TODO: nicer interface than r->fr->extent()? could we put the extent in the frame struct?
        cb.setViewport(
            0,
            vk::Viewport{
                0, 0, (float)r->fr->extent().width, (float)r->fr->extent().height, 0.f, 1.f}
        );
        cb.setScissor(0, vk::Rect2D{vk::Offset2D{}, r->fr->extent()});

        algo->generate_commands(cb, scene_data_desc_set, [&](auto cb, auto pl) {
            this->generate_scene_draw_commands(cb, pl);
        });

        cb.end();
    }

    frame.frame_cmd_buf.beginRenderPass(
        algo->get_render_pass_begin_info(frame.frame_index),
        vk::SubpassContents::eSecondaryCommandBuffers
    );
    frame.frame_cmd_buf.executeCommands(scene_render_cmd_buffer.get());
    frame.frame_cmd_buf.endRenderPass();
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
