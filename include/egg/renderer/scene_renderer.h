#pragma once
#include "egg/renderer/core/frame_renderer.h"
#include "egg/renderer/memory.h"
#include "renderer.h"

/*
 * transforms (can change once per frame)
 * materials (changes infrequently, technically static except editor)
 * textures (static if you load all assets up front)
 * geometry (static if you load all assets up front)
 */

struct texture {
    std::unique_ptr<gpu_image> img;

    texture(std::unique_ptr<gpu_image> img) : img(std::move(img)) {}
};

class scene_renderer {
    std::shared_ptr<asset_bundle>           current_bundle;
    std::unique_ptr<gpu_buffer>             vertex_buffer, index_buffer, staging_buffer;
    std::unordered_map<texture_id, texture> textures;

    void load_geometry_from_bundle(VmaAllocator allocator, vk::CommandBuffer upload_cmds);
    void load_textures_from_bundle(VmaAllocator allocator, vk::CommandBuffer upload_cmds);

  public:
    scene_renderer(flecs::world& world, std::unique_ptr<render_pipeline> pipeline);
    void start_resource_upload(
        renderer* r, std::shared_ptr<asset_bundle> bundle, vk::CommandBuffer upload_cmds
    );
    void resource_upload_cleanup();
    void render_frame(frame& frame);
};
