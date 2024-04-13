#include "egg/renderer/scene_renderer.h"
#include "egg/components.h"
#include "egg/renderer/imgui_renderer.h"
#include "imgui.h"
#include <iostream>
#include <stdexcept>
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
 * move swap_chain.cpp -> frame_renderer.cpp +
 * move device.cpp -> renderer.cpp ~
 * move static resources into seperate class +
 * move ECS setup into seperate scene_renderer function +
 * ****/

// future cool things:
// use compute shaders for mesh skinning, marching cubes for metaballs -> mercury!
//  could find a compute queue that is preferentially on a different GPU for parallel compute,
//  downside is GPU-to-GPU copy

using glm::vec2;
using glm::vec3;

scene_renderer::scene_renderer(
    renderer* _r, std::shared_ptr<flecs::world> _world, rendering_algorithm* _algo
)
    : r(_r), world(std::move(_world)),
      supported_depth_formats{
          vk::Format::eD16Unorm,
          vk::Format::eD16UnormS8Uint,
          vk::Format::eD24UnormS8Uint,
          vk::Format::eX8D24UnormPack32,
          vk::Format::eD32Sfloat,
          vk::Format::eD32SfloatS8Uint
      },
      algo(_algo), features(algo->required_features()),
      transforms(r->allocator, 384, vk::BufferUsageFlagBits::eStorageBuffer),
      shader_uniforms(r->allocator, vk::BufferUsageFlagBits::eUniformBuffer),
      gpu_lights(r->allocator, 16, vk::BufferUsageFlagBits::eStorageBuffer, true),
      should_regenerate_command_buffer(true) {
    for(auto i = supported_depth_formats.begin(); i != supported_depth_formats.end();) {
        auto props = r->phy_dev.getFormatProperties(*i);
        if((props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
           != vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
            i = supported_depth_formats.erase(i);
        } else {
            std::cout << "supported depth format: " << vk::to_string(*i) << "\n";
            i++;
        }
    }

    if(supported_depth_formats.empty())
        throw std::runtime_error("no supported depth formats available");

    algo->init_with_device(r->dev.get(), r->allocator, supported_depth_formats);

    auto buffers = r->dev->allocateCommandBuffersUnique(
        vk::CommandBufferAllocateInfo{r->command_pool.get(), vk::CommandBufferLevel::eSecondary, 1}
    );
    scene_render_cmd_buffer = std::move(buffers[0]);

    setup_ecs();

    surface_color_attachment = vk::AttachmentDescription{
        vk::AttachmentDescriptionFlags(),
        r->surface_format.format,
        vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::ePresentSrcKHR
    };

    algo->create_static_objects(surface_color_attachment);

    r->imgui()->add_window("Renderer/GPU Transforms", [&](bool* open) {
        if(ImGui::Begin("Renderer/GPU Transforms", open)) {
            if(ImGui::BeginTable("#TransformTable", 2, ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Index");
                ImGui::TableSetupColumn("Matrix");
                auto [start_index, end_index] = transforms.current_valid_range();
                auto* data                    = (mat4*)transforms.cpu_mapped();
                ImGui::TableHeadersRow();
                for(size_t i = start_index; i < end_index; ++i) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%zu", i);
                    ImGui::TableNextColumn();
                    ImGui::PushID(i);
                    ImGui::DragFloat4("c0", (float*)&data[i][0]);
                    ImGui::DragFloat4("c1", (float*)&data[i][1]);
                    ImGui::DragFloat4("c2", (float*)&data[i][2]);
                    ImGui::DragFloat4("c3", (float*)&data[i][3]);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    });
}

scene_renderer::~scene_renderer() {
    // unregister observers before the world gets destroyed since they have references to this
    for(auto ob : observers)
        ob.destruct();
    world.reset();

    delete algo;
}

void scene_renderer::setup_ecs() {
    observers.emplace_back(
        world->observer<comp::position, comp::rotation>()
            .term<comp::light>()
            .not_()
            .event(flecs::OnAdd)
            .event(flecs::OnRemove)
            .each([&](flecs::iter& it, size_t i, const comp::position& p, const comp::rotation& r) {
                if(it.event() == flecs::OnAdd) {
                    // std::cout << "add transform\n";
                    auto [t, bi] = transforms.alloc();
                    comp::gpu_transform gt{.transform = t, .gpu_index = bi};
                    it.entity(i).set<comp::gpu_transform>(gt);
                } else if(it.event() == flecs::OnRemove) {
                    transforms.free(it.entity(i).get<comp::gpu_transform>()->gpu_index);
                    it.entity(i).remove<comp::gpu_transform>();
                }
            })
    );
    observers.emplace_back(world->observer<comp::position, comp::rotation, comp::gpu_transform>()
                               .event(flecs::OnSet)
                               .each([&](flecs::iter&          it,
                                         size_t                i,
                                         const comp::position& p,
                                         const comp::rotation& r,
                                         comp::gpu_transform&  t) {
                                   std::optional<mat4> s;
                                   const auto*         obj = it.entity(i).get<comp::renderable>();
                                   if(obj != nullptr)
                                       s = current_bundle->object_transform(obj->object);
                                   t.update(p, r, s);
                                   if(it.entity(i).has<tag::active_camera>()) {
                                       *t.transform                = inverse(*t.transform);
                                       shader_uniforms->camera_pos = p.pos;
                                   }
                               }));
    observers.emplace_back(world->observer<comp::camera>()
                               .event(flecs::OnSet)
                               .event(flecs::OnRemove)
                               .each([&](flecs::iter& it, size_t i, comp::camera& cam) {
                                   if(it.event() == flecs::OnSet) {
                                       if(cam.proj_transform.first == nullptr)
                                           cam.proj_transform = transforms.alloc();
                                       cam.update(r->fr->aspect_ratio());
                                   } else if(it.event() == flecs::OnRemove) {
                                       transforms.free(cam.proj_transform.second);
                                   }
                               }));
    observers.emplace_back(world->observer<comp::gpu_transform, comp::renderable>()
                               .event(flecs::OnAdd)
                               .event(flecs::OnSet)
                               .event(flecs::OnRemove)
                               .iter([&](flecs::iter&, comp::gpu_transform*, comp::renderable*) {
                                   should_regenerate_command_buffer = true;
                               }));
    observers.emplace_back(world->observer<comp::light>()
                               .event(flecs::OnRemove)
                               .each([&](flecs::iter& it, size_t i, comp::light& lgh) {
                                   lgh.gpu_info.first->type = light_type::invalid;
                                   gpu_lights.free(lgh.gpu_info.second);
                               }));
    observers.emplace_back(
        world->observer<comp::light, const comp::directional_light, const comp::rotation>()
            .event(flecs::OnSet)
            .each([&](comp::light& lgh, const comp::directional_light& _, const comp::rotation& rot
                  ) {
                if(lgh.gpu_info.first == nullptr) lgh.gpu_info = gpu_lights.alloc();
                *lgh.gpu_info.first = light_info{
                    .emittance = lgh.emittance,
                    .type      = light_type::directional,
                    .direction = glm::rotate(rot.rot, vec3(0.f, 0.f, 1.f))
                };
            })
    );
    observers.emplace_back(world
                               ->observer<
                                   comp::light,
                                   const comp::spot_light,
                                   const comp::position,
                                   const comp::rotation>()
                               .event(flecs::OnSet)
                               .each([&](comp::light&            lgh,
                                         const comp::spot_light& params,
                                         const comp::position&   pos,
                                         const comp::rotation&   rot) {
                                   if(lgh.gpu_info.first == nullptr)
                                       lgh.gpu_info = gpu_lights.alloc();
                                   *lgh.gpu_info.first = light_info{
                                       .emittance = lgh.emittance,
                                       .type      = light_type::spot,
                                       .position  = pos.pos,
                                       .param1    = params.inner_cutoff,
                                       .direction = glm::rotate(rot.rot, vec3(0.f, 0.f, 1.f)),
                                       .param2    = params.outer_cutoff,
                                   };
                               }));
    observers.emplace_back(
        world->observer<comp::light, const comp::point_light, const comp::position>()
            .event(flecs::OnSet)
            .each([&](comp::light& lgh, const comp::point_light& params, const comp::position& pos
                  ) {
                if(lgh.gpu_info.first == nullptr) lgh.gpu_info = gpu_lights.alloc();
                *lgh.gpu_info.first = light_info{
                    .emittance = lgh.emittance,
                    .type      = light_type::point,
                    .position  = pos.pos,
                    .param1    = params.attenuation,
                };
            })
    );
    active_camera_q = world->query<tag::active_camera, comp::gpu_transform, comp::camera>();
    renderable_q    = world->query<comp::gpu_transform, comp::renderable>();
}

const vk::PushConstantRange scene_data_push_consts{
    vk::ShaderStageFlagBits::eAll, 0, sizeof(uint32_t) * 2 + sizeof(per_object_push_constants)
};

rendering_algorithm* scene_renderer::swap_rendering_algorithm(rendering_algorithm* new_algo) {
    std::cout << "swapping in new rendering algorithm\n";
    if(!scene_data)
        throw std::runtime_error("cannot replace rendering algorithm before scene data is loaded");
    auto* old_algo = algo;
    algo           = new_algo;
    // TODO: make sure that the new algo is feature compatable with the old one
    std::cout << "\tinitalizing with device\n";
    algo->init_with_device(r->dev.get(), r->allocator, supported_depth_formats);
    std::cout << "\tcreating static objects\n";
    algo->create_static_objects(surface_color_attachment);
    std::cout << "\tcreating pipeline layouts\n";
    algo->create_pipeline_layouts(scene_data->desc_set_layout.get(), scene_data_push_consts);
    std::cout << "\tcreating pipelines\n";
    algo->create_pipelines();
    std::cout << "\tcreating framebuffers\n";
    algo->create_framebuffers(r->fr);
    should_regenerate_command_buffer = true;
    return old_algo;
}

void scene_renderer::start_resource_upload(
    const std::shared_ptr<asset_bundle>& bundle, vk::CommandBuffer upload_cmds
) {
    current_bundle = bundle;

    scene_data = std::make_unique<gpu_static_scene_data>(r, current_bundle, upload_cmds, features);
}

void scene_renderer::setup_scene_post_upload() {
    std::vector<vk::WriteDescriptorSet> writes;

    auto img_infos = scene_data->setup_descriptors(r, current_bundle.get(), writes);

    vk::DescriptorBufferInfo transforms_buffer_info{transforms.get(), 0, VK_WHOLE_SIZE};
    writes.emplace_back(
        scene_data->desc_set,
        0,
        0,
        1,
        vk::DescriptorType::eStorageBuffer,
        nullptr,
        &transforms_buffer_info
    );

    vk::DescriptorBufferInfo uniforms_buffer_info{shader_uniforms.get(), 0, VK_WHOLE_SIZE};
    writes.emplace_back(
        scene_data->desc_set,
        2,
        0,
        1,
        vk::DescriptorType::eUniformBuffer,
        nullptr,
        &uniforms_buffer_info
    );

    vk::DescriptorBufferInfo lights_buffer_info{gpu_lights.get(), 0, VK_WHOLE_SIZE};
    writes.emplace_back(
        scene_data->desc_set,
        3,
        0,
        1,
        vk::DescriptorType::eStorageBuffer,
        nullptr,
        &lights_buffer_info
    );

    r->dev->updateDescriptorSets(writes.size(), writes.data(), 0, nullptr);

    algo->create_pipeline_layouts(scene_data->desc_set_layout.get(), scene_data_push_consts);

    algo->create_pipelines();
}

void scene_renderer::resource_upload_cleanup() { scene_data->resource_upload_cleanup(); }

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
    cb.bindVertexBuffers(0, scene_data->vertex_buffer->get(), {0});
    cb.bindIndexBuffer(scene_data->index_buffer->get(), 0, vk::IndexType::eUint32);
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
        std::cout << "regenerating command buffers\n";

        auto cb = scene_render_cmd_buffer.get();
        cb.begin(vk::CommandBufferBeginInfo{
            vk::CommandBufferUsageFlagBits::eSimultaneousUse
                | vk::CommandBufferUsageFlagBits::eRenderPassContinue,
            algo->get_command_buffer_inheritance_info()
        });

        // TODO: nicer interface than r->fr->extent()? could we put the extent in the frame struct?
        cb.setViewport(
            0,
            vk::Viewport{
                0, 0, (float)r->fr->extent().width, (float)r->fr->extent().height, 0.f, 1.f
            }
        );
        cb.setScissor(0, vk::Rect2D{vk::Offset2D{}, r->fr->extent()});

        algo->generate_commands(
            cb,
            scene_data->desc_set,
            [&](auto cb, auto pl) { this->generate_scene_draw_commands(cb, pl); },
            [&](vk::CommandBuffer cb, auto pl) {
                // assume that generate_scene_draw_commands() has already been called
                cb.bindVertexBuffers(0, scene_data->cube_vertex_buffer->get(), {0});
                cb.bindIndexBuffer(scene_data->cube_index_buffer->get(), 0, vk::IndexType::eUint16);
                cb.drawIndexed(36, 1, 0, 0, 0);
                // cb.draw(3, 1, 0, 0);
            }
        );

        cb.end();
    }

    frame.frame_cmd_buf.beginRenderPass(
        algo->get_render_pass_begin_info(frame.frame_index),
        vk::SubpassContents::eSecondaryCommandBuffers
    );
    frame.frame_cmd_buf.executeCommands(scene_render_cmd_buffer.get());
    frame.frame_cmd_buf.endRenderPass();
}
