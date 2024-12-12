#include "asset-bundler/format.h"
#include "egg/renderer/imgui_renderer.h"
#include "egg/renderer/scene_renderer.h"
#include "mem_arena.h"
#include <vulkan/vulkan_format_traits.hpp>

const size_t CUBE_VERTEX_COUNT = 24;
const size_t CUBE_INDEX_COUNT  = 36;
const size_t CUBE_TOTAL_SIZE
    = sizeof(vec3) * CUBE_VERTEX_COUNT + sizeof(uint16_t) * CUBE_INDEX_COUNT;

void generate_cube(
    float                                       width,
    float                                       height,
    float                                       depth,
    std::function<void(vec3, vec3, vec3, vec2)> vertex,
    std::function<void(size_t)>                 index
) {
    // this is an antique at this point.

    float w2 = 0.5f * width;
    float h2 = 0.5f * height;
    float d2 = 0.5f * depth;

    vertex({-w2, -h2, -d2}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f});
    vertex({-w2, +h2, -d2}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f});
    vertex({+w2, +h2, -d2}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f});
    vertex({+w2, -h2, -d2}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f});

    vertex({-w2, -h2, +d2}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    vertex({+w2, -h2, +d2}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f});
    vertex({+w2, +h2, +d2}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f});
    vertex({-w2, +h2, +d2}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f});

    vertex(
        {-w2, +h2, -d2},
        {0.0f, 1.0f, 0.0f},
        {
            1.0f,
            0.0f,
            0.0f,
        },
        {0.0f, 1.0f}
    );
    vertex(
        {-w2, +h2, +d2},
        {0.0f, 1.0f, 0.0f},
        {
            1.0f,
            0.0f,
            0.0f,
        },
        {0.0f, 0.0f}
    );
    vertex(
        {+w2, +h2, +d2},
        {0.0f, 1.0f, 0.0f},
        {
            1.0f,
            0.0f,
            0.0f,
        },
        {1.0f, 0.0f}
    );
    vertex(
        {+w2, +h2, -d2},
        {0.0f, 1.0f, 0.0f},
        {
            1.0f,
            0.0f,
            0.0f,
        },
        {1.0f, 1.0f}
    );

    vertex({-w2, -h2, -d2}, {0.0f, -1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
    vertex({+w2, -h2, -d2}, {0.0f, -1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f});
    vertex({+w2, -h2, +d2}, {0.0f, -1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f});
    vertex({-w2, -h2, +d2}, {0.0f, -1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f});

    vertex({-w2, -h2, +d2}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f});
    vertex({-w2, +h2, +d2}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f});
    vertex({-w2, +h2, -d2}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f});
    vertex({-w2, -h2, -d2}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f});

    vertex(
        {+w2, -h2, -d2},
        {
            1.0f,
            0.0f,
            0.0f,
        },
        {0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f}
    );
    vertex(
        {+w2, +h2, -d2},
        {
            1.0f,
            0.0f,
            0.0f,
        },
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f}
    );
    vertex(
        {+w2, +h2, +d2},
        {
            1.0f,
            0.0f,
            0.0f,
        },
        {0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f}
    );
    vertex(
        {+w2, -h2, +d2},
        {
            1.0f,
            0.0f,
            0.0f,
        },
        {0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f}
    );

    index(0);
    index(1);
    index(2);
    index(0);
    index(2);
    index(3);

    index(4);
    index(5);
    index(6);
    index(4);
    index(6);
    index(7);

    index(8);
    index(9);
    index(10);
    index(8);
    index(10);
    index(11);

    index(12);
    index(13);
    index(14);
    index(12);
    index(14);
    index(15);

    index(16);
    index(17);
    index(18);
    index(16);
    index(18);
    index(19);

    index(20);
    index(21);
    index(22);
    index(20);
    index(22);
    index(23);
}

gpu_static_scene_data::gpu_static_scene_data(
    renderer*                            r,
    const std::shared_ptr<asset_bundle>& bundle,
    vk::CommandBuffer                    upload_cmds,
    const renderer_features&             features
) {
    staging_buffer = std::make_unique<gpu_buffer>(
        r->gpu_alloc(),
        vk::BufferCreateInfo{
            {}, bundle->gpu_data_size() + CUBE_TOTAL_SIZE, vk::BufferUsageFlagBits::eTransferSrc
        },
        VmaAllocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                     | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO
        }
    );
    staging_buffer->set_debug_name(r->vulkan_instance(), r->device(), "staging buffer");

    bundle->take_gpu_data((uint8_t*)staging_buffer->cpu_mapped());

    // generate skybox geometry
    uint8_t* cube_ptr = (uint8_t*)staging_buffer->cpu_mapped() + bundle->gpu_data_size();
    generate_cube(
        1.f,
        1.f,
        1.f,
        [&](vec3 p, auto, auto, auto) {
            *((vec3*)cube_ptr) = p;
            cube_ptr += sizeof(vec3);
        },
        [&](size_t i) {
            *((uint16_t*)cube_ptr) = i;
            cube_ptr += sizeof(uint16_t);
        }
    );

    load_geometry_from_bundle(r, bundle.get(), upload_cmds, features);
    create_textures_from_bundle(r, bundle.get());
    generate_upload_commands_for_textures(bundle.get(), upload_cmds);
    create_envs_from_bundle(r, bundle.get());
    generate_upload_commands_for_envs(bundle.get(), upload_cmds);
    if(features.raytracing) build_object_accel_structs(r, bundle.get(), upload_cmds);

    r->imgui()->add_window("Static Resources", [this, bundle](bool* open) {
        this->texture_window_gui(open, bundle);
    });
}

void gpu_static_scene_data::load_geometry_from_bundle(
    renderer*                r,
    asset_bundle*            current_bundle,
    vk::CommandBuffer        upload_cmds,
    const renderer_features& features
) {
    auto allocator = r->gpu_alloc();

    vk::BufferUsageFlags geometry_usage_bits = vk::BufferUsageFlagBits::eTransferDst;
    if(features.raytracing) {
        geometry_usage_bits
            |= vk::BufferUsageFlagBits::eShaderDeviceAddress
               | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
    }

    const auto& bh          = current_bundle->bundle_header();
    auto        vertex_size = bh.num_total_vertices * sizeof(vertex);
    vertex_buffer           = std::make_unique<gpu_buffer>(
        allocator,
        vk::BufferCreateInfo{
                      {}, vertex_size, vk::BufferUsageFlagBits::eVertexBuffer | geometry_usage_bits
        },
        VmaAllocationCreateInfo{
                      .usage = VMA_MEMORY_USAGE_AUTO,
        }
    );
    vertex_buffer->set_debug_name(r->vulkan_instance(), r->device(), "scene static vertex buffer");
    upload_cmds.copyBuffer(
        staging_buffer->get(),
        vertex_buffer->get(),
        vk::BufferCopy{bh.vertex_start_offset - bh.gpu_data_offset, 0, vertex_size}
    );

    auto index_size = bh.num_total_indices * sizeof(index_type);
    index_buffer    = std::make_unique<gpu_buffer>(
        allocator,
        vk::BufferCreateInfo{
               {}, index_size, vk::BufferUsageFlagBits::eIndexBuffer | geometry_usage_bits
        },
        VmaAllocationCreateInfo{
               .usage = VMA_MEMORY_USAGE_AUTO,
        }
    );
    index_buffer->set_debug_name(r->vulkan_instance(), r->device(), "scene static index buffer");

    upload_cmds.copyBuffer(
        staging_buffer->get(),
        index_buffer->get(),
        vk::BufferCopy{bh.index_start_offset - bh.gpu_data_offset, 0, index_size}
    );

    cube_vertex_buffer = std::make_unique<gpu_buffer>(
        allocator,
        vk::BufferCreateInfo{
            {},
            CUBE_VERTEX_COUNT * sizeof(vec3),
            vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst
        },
        VmaAllocationCreateInfo{
            .usage = VMA_MEMORY_USAGE_AUTO,
        }
    );
    cube_vertex_buffer->set_debug_name(r->vulkan_instance(), r->device(), "cube vertex buffer");

    upload_cmds.copyBuffer(
        staging_buffer->get(),
        cube_vertex_buffer->get(),
        vk::BufferCopy{current_bundle->gpu_data_size(), 0, CUBE_VERTEX_COUNT * sizeof(vec3)}
    );

    cube_index_buffer = std::make_unique<gpu_buffer>(
        allocator,
        vk::BufferCreateInfo{
            {},
            CUBE_INDEX_COUNT * sizeof(uint16_t),
            vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst
        },
        VmaAllocationCreateInfo{
            .usage = VMA_MEMORY_USAGE_AUTO,
        }
    );
    cube_index_buffer->set_debug_name(r->vulkan_instance(), r->device(), "cube index buffer");

    upload_cmds.copyBuffer(
        staging_buffer->get(),
        cube_index_buffer->get(),
        vk::BufferCopy{
            current_bundle->gpu_data_size() + CUBE_VERTEX_COUNT * sizeof(vec3),
            0,
            CUBE_INDEX_COUNT * sizeof(uint16_t)
        }
    );
}

vk::ImageCreateInfo create_info_for_image(
    const asset_bundle_format::image& img,
    vk::ImageUsageFlags               usage,
    vk::ImageCreateFlags              flags = {}
) {
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

texture::texture(
    renderer*                         r,
    const asset_bundle_format::image& img_info,
    vk::ImageViewType                 type,
    vk::ImageCreateFlags              flags
) {
    if(type == vk::ImageViewType::eCube) flags = flags | vk::ImageCreateFlagBits::eCubeCompatible;

    img = std::make_unique<gpu_image>(
        r->gpu_alloc(),
        create_info_for_image(
            img_info, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, flags
        ),
        VmaAllocationCreateInfo{.usage = VMA_MEMORY_USAGE_AUTO}
    );

    auto subres_range = vk::ImageSubresourceRange(
        vk::ImageAspectFlagBits::eColor, 0, img_info.mip_levels, 0, img_info.array_layers
    );

    img_view = r->device().createImageViewUnique(vk::ImageViewCreateInfo{
        {}, img->get(), type, vk::Format(img_info.format), vk::ComponentMapping{}, subres_range
    });

    imgui_id = r->imgui()->add_texture(img_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal);
}

void gpu_static_scene_data::create_textures_from_bundle(renderer* r, asset_bundle* current_bundle) {
    for(texture_id i = 0; i < current_bundle->num_textures(); ++i) {
        const auto& th = current_bundle->texture_by_index(i);
        textures.emplace(th.id, texture{r, th.img, vk::ImageViewType::e2D});
    }
}

void gen_transfer_barriers(
    const texture&                       t,
    std::vector<vk::ImageMemoryBarrier>& undef_to_transfer_barriers,
    std::vector<vk::ImageMemoryBarrier>& transfer_to_shader_read_barriers
) {
    auto subres_range = vk::ImageSubresourceRange(
        vk::ImageAspectFlagBits::eColor, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS
    );
    undef_to_transfer_barriers.emplace_back(
        vk::AccessFlags(),
        vk::AccessFlagBits::eTransferWrite,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        t.img->get(),
        subres_range
    );
    transfer_to_shader_read_barriers.emplace_back(
        vk::AccessFlagBits::eTransferWrite,
        vk::AccessFlagBits::eShaderRead,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        t.img->get(),
        subres_range
    );
}

void gpu_static_scene_data::generate_upload_commands_for_texture(
    asset_bundle*                     current_bundle,
    vk::CommandBuffer                 upload_cmds,
    const texture&                    t,
    const asset_bundle_format::image& img,
    size_t                            bundle_offset
) const {
    size_t offset  = bundle_offset - current_bundle->bundle_header().gpu_data_offset;
    auto   regions = copy_regions_for_linear_image2d(
        img.width, img.height, img.mip_levels, img.array_layers, (vk::Format)img.format, offset
    );

    upload_cmds.copyBufferToImage(
        staging_buffer->get(), t.img->get(), vk::ImageLayout::eTransferDstOptimal, regions
    );
}

void gpu_static_scene_data::generate_upload_commands_for_textures(
    asset_bundle* current_bundle, vk::CommandBuffer upload_cmds
) {
    std::vector<vk::ImageMemoryBarrier> undef_to_transfer_barriers,
        transfer_to_shader_read_barriers;
    for(const auto& [id, tx] : textures)
        gen_transfer_barriers(tx, undef_to_transfer_barriers, transfer_to_shader_read_barriers);

    upload_cmds.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer,
        {},
        {},
        {},
        undef_to_transfer_barriers
    );

    for(texture_id i = 0; i < current_bundle->num_textures(); ++i) {
        const auto& th = current_bundle->texture_by_index(i);

        generate_upload_commands_for_texture(
            current_bundle, upload_cmds, textures.at(th.id), th.img, th.offset
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

void gpu_static_scene_data::create_envs_from_bundle(renderer* r, asset_bundle* current_bundle) {
    for(size_t i = 0; i < current_bundle->num_environments(); ++i) {
        const auto& ev = current_bundle->environment_by_index(i);
        envs.emplace(
            ev.name,
            environment{
                .sky                = texture{r, ev.skybox,             vk::ImageViewType::eCube},
                .diffuse_irradiance = texture{r, ev.diffuse_irradiance, vk::ImageViewType::eCube}
        }
        );
        current_env = ev.name;
    }
}

void gpu_static_scene_data::generate_upload_commands_for_envs(
    asset_bundle* current_bundle, vk::CommandBuffer upload_cmds
) {
    std::vector<vk::ImageMemoryBarrier> undef_to_transfer_barriers,
        transfer_to_shader_read_barriers;
    for(const auto& [_, ev] : envs) {
        gen_transfer_barriers(ev.sky, undef_to_transfer_barriers, transfer_to_shader_read_barriers);
        gen_transfer_barriers(
            ev.diffuse_irradiance, undef_to_transfer_barriers, transfer_to_shader_read_barriers
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

    for(size_t i = 0; i < current_bundle->num_environments(); ++i) {
        const auto& ev = current_bundle->environment_by_index(i);

        generate_upload_commands_for_texture(
            current_bundle, upload_cmds, envs.at(ev.name).sky, ev.skybox, ev.skybox_offset
        );
        generate_upload_commands_for_texture(
            current_bundle,
            upload_cmds,
            envs.at(ev.name).diffuse_irradiance,
            ev.diffuse_irradiance,
            ev.diffuse_irradiance_offset
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

std::vector<vk::DescriptorImageInfo> gpu_static_scene_data::setup_descriptors(
    renderer* r, asset_bundle* current_bundle, std::vector<vk::WriteDescriptorSet>& writes
) {
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
        16.f
    });

    // TODO: we should create the descriptor set layout in the scene_renderer?
    vk::DescriptorSetLayoutBinding bindings[] = {
  // transforms storage buffer
        {0, vk::DescriptorType::eStorageBuffer,        1, vk::ShaderStageFlagBits::eAll     },
 // scene textures
        {1,
         vk::DescriptorType::eCombinedImageSampler,
         (uint32_t)current_bundle->num_textures(),
         vk::ShaderStageFlagBits::eAll                                                      },
 // per-frame shader uniforms
        {2, vk::DescriptorType::eUniformBuffer,        1, vk::ShaderStageFlagBits::eAll     },
 // lights storage buffer
        {3, vk::DescriptorType::eStorageBuffer,        1, vk::ShaderStageFlagBits::eAll     },
 // environment skybox
        {4, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}
    };

    desc_set_layout = r->device().createDescriptorSetLayoutUnique(
        vk::DescriptorSetLayoutCreateInfo{{}, sizeof(bindings) / sizeof(bindings[0]), bindings}
    );

    vk::DescriptorPoolSize pool_sizes[] = {
        {vk::DescriptorType::eStorageBuffer,        2                                           },
        {vk::DescriptorType::eUniformBuffer,        1                                           },
        {vk::DescriptorType::eCombinedImageSampler, (uint32_t)current_bundle->num_textures() + 1},
    };
    desc_set_pool = r->device().createDescriptorPoolUnique(
        vk::DescriptorPoolCreateInfo{{}, 1, sizeof(pool_sizes) / sizeof(pool_sizes[0]), pool_sizes}
    );

    desc_set = r->device().allocateDescriptorSets(
        vk::DescriptorSetAllocateInfo{desc_set_pool.get(), desc_set_layout.get()}
    )[0];

    std::vector<vk::DescriptorImageInfo> texture_infos;
    texture_infos.reserve(current_bundle->num_textures() + 1);
    size_t i;
    for(i = 0; i < current_bundle->num_textures(); ++i) {
        // TODO: again the assumption is that texture IDs are contiguous from 1
        const auto& tx = textures.at(i + 1);
        writes.emplace_back(
            desc_set, 1, i, 1, vk::DescriptorType::eCombinedImageSampler, texture_infos.data() + i
        );
        texture_infos.emplace_back(
            texture_sampler.get(), tx.img_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal
        );
    }

    // environment map skybox
    writes.emplace_back(
        desc_set, 4, 0, 1, vk::DescriptorType::eCombinedImageSampler, texture_infos.data() + i
    );
    texture_infos.emplace_back(
        texture_sampler.get(),
        envs.at(current_env).sky.img_view.get(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    );

    return texture_infos;
}

void gpu_static_scene_data::build_object_accel_structs(
    renderer* r, asset_bundle* current_bundle, vk::CommandBuffer upload_cmds
) {
    std::cout << "build_object_accel_structs\n";
    // create infos for each object's geometry and query for the size of the AS.
    arena<vk::AccelerationStructureBuildRangeInfoKHR>          ranges;
    std::vector<vk::AccelerationStructureBuildGeometryInfoKHR> build_geoinfos;
    std::vector<vk::AccelerationStructureBuildSizesInfoKHR>    build_sizeinfos;
    std::vector<vk::AccelerationStructureBuildRangeInfoKHR*>   build_rangeinfos;
    vk::DeviceSize total_accel_struct_size = 0, max_build_scratch_size = 0;

    auto vertex_buffer_addr = vertex_buffer->device_address(r->device());
    auto index_buffer_addr  = index_buffer->device_address(r->device());

    std::cout << std::hex << "vertex buffer addr = " << vertex_buffer_addr << "\n"
              << "index buffer addr = " << index_buffer_addr << "\n"
              << std::dec;

    auto geometry = vk::AccelerationStructureGeometryKHR{
        vk::GeometryTypeKHR::eTriangles,
        vk::AccelerationStructureGeometryTrianglesDataKHR{
                                                          vertex_attribute_description[0].format,
                                                          vertex_buffer_addr, sizeof(vertex) + vertex_attribute_description[0].offset,
                                                          (uint32_t)current_bundle->bundle_header().num_total_vertices,
                                                          vk::IndexType::eUint32,
                                                          index_buffer_addr
        }
    };

    size_t max_num_meshes = 0;
    for(size_t i = 0; i < current_bundle->num_objects(); ++i)
        max_num_meshes = glm::max(max_num_meshes, current_bundle->object_meshes(i).len());

    // create an array of pointers to the same geometry structure that is long enough that we can
    // reuse it for all objects
    auto geos = std::vector<vk::AccelerationStructureGeometryKHR*>(max_num_meshes, &geometry);

    for(size_t i = 0; i < current_bundle->num_objects(); ++i) {
        auto                  meshes = current_bundle->object_meshes(i);
        auto*                 rs     = ranges.alloc_array(meshes.len());
        std::vector<uint32_t> primitive_counts;
        for(size_t m = 0; meshes.has_more(); ++m, ++meshes) {
            rs[m] = vk::AccelerationStructureBuildRangeInfoKHR{
                (uint32_t)meshes->index_count / 3,
                (uint32_t)(meshes->index_offset * sizeof(index_type)),
                (uint32_t)meshes->vertex_offset,
            };
            primitive_counts.emplace_back(rs[m].primitiveCount);
        }
        build_rangeinfos.push_back(rs);
        build_geoinfos.emplace_back(vk::AccelerationStructureBuildGeometryInfoKHR{
            vk::AccelerationStructureTypeKHR::eBottomLevel,
            vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
            | vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction,
            vk::BuildAccelerationStructureModeKHR::eBuild,
            {},
            {},
            (uint32_t)primitive_counts.size(),
            nullptr,
            geos.data()
        });
        auto sizeinfo = r->device().getAccelerationStructureBuildSizesKHR(
            vk::AccelerationStructureBuildTypeKHR::eDevice, build_geoinfos[i], primitive_counts
        );
        // make sure we have enough room for alignment padding
        total_accel_struct_size += (sizeinfo.accelerationStructureSize + 255) & ~255;
        max_build_scratch_size = glm::max(max_build_scratch_size, sizeinfo.buildScratchSize);
        build_sizeinfos.emplace_back(sizeinfo);
    }

    // create buffer to store AS, and then create each AS in the buffer
    object_accel_struct_storage = std::make_unique<gpu_buffer>(
        r->gpu_alloc(),
        vk::BufferCreateInfo{
            {},
            total_accel_struct_size,
            vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR,
        }
    );
    object_accel_struct_storage->set_debug_name(
        r->vulkan_instance(), r->device(), "RT object BLAS storage"
    );

    vk::AccelerationStructureCreateInfoKHR accel_struct_cfo{
        {}, object_accel_struct_storage->get(), 0, 0, vk::AccelerationStructureTypeKHR::eBottomLevel
    };
    for(size_t i = 0; i < current_bundle->num_objects(); ++i) {
        accel_struct_cfo.setSize(build_sizeinfos[i].accelerationStructureSize);
        std::cout << "creating BLAS " << i << " size=" << accel_struct_cfo.size
                  << " offset=" << std::hex << accel_struct_cfo.offset << "\n"
                  << std::dec;
        object_accel_structs.emplace_back(
            r->device().createAccelerationStructureKHRUnique(accel_struct_cfo)
        );
        // offsets must be a multiple of 256
        accel_struct_cfo.setOffset(
            (accel_struct_cfo.offset + build_sizeinfos[i].accelerationStructureSize + 255) & ~255
        );
    }

    // create stratch buffer
    accel_scratch = std::make_unique<gpu_buffer>(
        r->gpu_alloc(),
        vk::BufferCreateInfo{
            {},
            max_build_scratch_size,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        }
    );
    accel_scratch->set_debug_name(r->vulkan_instance(), r->device(), "RT AS build scratch");

    auto accel_scratch_addr = accel_scratch->device_address(r->device());

    // update build infos to point to our new objects
    for(size_t i = 0; i < build_geoinfos.size(); ++i) {
        build_geoinfos[i].setDstAccelerationStructure(object_accel_structs[i].get());
        build_geoinfos[i].setScratchData(accel_scratch_addr);
    }

    std::cout << "total_accel_struct_size = " << total_accel_struct_size << "\n"
              << "max_build_scratch_size = " << max_build_scratch_size << "\n"
              << "scratch addr = " << std::hex << accel_scratch_addr << std::dec << "\n";

    // add the commands to build the AS
    // make sure the input buffers are ready before the build
    std::array<vk::BufferMemoryBarrier2, 2> buffer_barriers;
    buffer_barriers.fill(vk::BufferMemoryBarrier2{
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite,
        vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
        vk::AccessFlagBits2::eShaderRead,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        VK_NULL_HANDLE,
        0,
        VK_WHOLE_SIZE
    });
    buffer_barriers[0].setBuffer(vertex_buffer->get());
    buffer_barriers[1].setBuffer(index_buffer->get());
    upload_cmds.pipelineBarrier2(
        vk::DependencyInfo{{}, 0, nullptr, buffer_barriers.size(), buffer_barriers.data()}
    );
    // build one at a time to avoid memory aliasing within `buildAccelerationStructuresKHR`
    // commands.
    std::array<vk::BufferMemoryBarrier2, 2> per_build_buffer_barriers;
    per_build_buffer_barriers.fill(vk::BufferMemoryBarrier2{
        vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
        vk::AccessFlagBits2::eAccelerationStructureWriteKHR,
        vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
        vk::AccessFlagBits2::eAccelerationStructureWriteKHR,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        VK_NULL_HANDLE,
        0,
        VK_WHOLE_SIZE
    });
    per_build_buffer_barriers[0].setBuffer(object_accel_struct_storage->get());
    per_build_buffer_barriers[1].setBuffer(accel_scratch->get());
    vk::DependencyInfo per_build_dependency{
        {}, 0, nullptr, per_build_buffer_barriers.size(), per_build_buffer_barriers.data()
    };
    for(size_t i = 0; i < build_geoinfos.size(); ++i) {
        upload_cmds.pipelineBarrier2(per_build_dependency);
        upload_cmds.buildAccelerationStructuresKHR(1, &build_geoinfos[i], &build_rangeinfos[i]);
    }
}

void gpu_static_scene_data::resource_upload_cleanup() {
    staging_buffer.reset();
    accel_scratch.reset();
}

void gpu_static_scene_data::texture_window_gui(
    bool* open, std::shared_ptr<asset_bundle> current_bundle
) {
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
                    ImGui::Text(
                        "%u x %u (%u mips, %u layers)",
                        th.img.width,
                        th.img.height,
                        th.img.mip_levels,
                        th.img.array_layers
                    );
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
                for(size_t i = 0; i < current_bundle->num_materials(); ++i) {
                    const auto& m = current_bundle->material(i);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    // TODO: when ImStrv lands we won't need this copy
                    auto name = std::string(current_bundle->string(m.name));
                    ImGui::Text("%s", name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text(
                        "B%u N%u R%u M%u", m.base_color, m.normals, m.roughness, m.metallic
                    );
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
            for(size_t gi = 0; gi < current_bundle->num_groups(); ++gi) {
                auto name     = std::string(current_bundle->string(current_bundle->group_name(gi)));
                auto g_bounds = current_bundle->group_bounds(gi);
                ImGui::PushID(gi);
                if(ImGui::TreeNodeEx(
                       "#",
                       ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick,
                       "%lu %s (%f,%f,%f):(%f,%f,%f)",
                       gi,
                       name.c_str(),
                       g_bounds.min.x,
                       g_bounds.min.y,
                       g_bounds.min.z,
                       g_bounds.max.x,
                       g_bounds.max.y,
                       g_bounds.max.z
                   )) {
                    for(auto ob = current_bundle->group_objects(gi); ob.has_more(); ++ob) {
                        auto ob_id = *ob;
                        auto name
                            = std::string(current_bundle->string(current_bundle->object_name(ob_id))
                            );
                        auto ob_bounds = current_bundle->object_bounds(ob_id);
                        if(ImGui::TreeNode(
                               (void*)ob_id,
                               "%u %s (%f,%f,%f):(%f,%f,%f)",
                               ob_id,
                               name.c_str(),
                               ob_bounds.min.x,
                               ob_bounds.min.y,
                               ob_bounds.min.z,
                               ob_bounds.max.x,
                               ob_bounds.max.y,
                               ob_bounds.max.z
                           )) {
                            size_t id = 0;
                            for(auto m = current_bundle->object_meshes(ob_id); m.has_more(); ++m) {
                                if(ImGui::TreeNodeEx(
                                       (void*)id,
                                       ImGuiTreeNodeFlags_Leaf,
                                       "mesh [V@%lx I@%lx:%lx M%lu, (%f,%f,%f):(%f,%f,%f)]",
                                       m->vertex_offset,
                                       m->index_offset,
                                       m->index_count,
                                       m->material_index,
                                       m->bounds.min.x,
                                       m->bounds.min.y,
                                       m->bounds.min.z,
                                       m->bounds.max.x,
                                       m->bounds.max.y,
                                       m->bounds.max.z
                                   ))
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
            if(ImGui::BeginTable("#env-table", 3)) {
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Skybox");
                ImGui::TableSetupColumn("Diffuse Irradiance Map");
                ImGui::TableHeadersRow();
                for(const auto& [name, ev] : envs) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    auto name_s = std::string(current_bundle->string(name));
                    ImGui::Text("%s", name_s.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Image((ImTextureID)ev.sky.imgui_id, ImVec2(256, 256));
                    ImGui::TableNextColumn();
                    ImGui::Image((ImTextureID)ev.diffuse_irradiance.imgui_id, ImVec2(256, 256));
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::End();
}
