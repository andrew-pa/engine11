#pragma once
#include "egg/renderer/core/frame_renderer.h"
#include "egg/renderer/memory.h"
#include "renderer.h"
#include <glm/glm.hpp>

/*
 * transforms (can change once per frame)
 * materials (changes infrequently, technically static except editor)
 * textures (static if you load all assets up front)
 * geometry (static if you load all assets up front)
 */

struct texture {
    std::unique_ptr<gpu_image> img;
    vk::UniqueImageView        img_view;
    uint64_t                   imgui_id;

    texture(std::unique_ptr<gpu_image> img, vk::UniqueImageView&& iv, uint64_t id)
        : img(std::move(img)), img_view(std::move(iv)), imgui_id(id) {}
};

class scene_renderer {
    std::shared_ptr<asset_bundle> current_bundle;

    // TODO: could move static resources into a seperate component to reduce complexity of the
    // scene_renderer itself
    std::unique_ptr<gpu_buffer>             vertex_buffer, index_buffer, staging_buffer;
    std::unordered_map<texture_id, texture> textures;
    void load_geometry_from_bundle(VmaAllocator allocator, vk::CommandBuffer upload_cmds);
    void create_textures_from_bundle(renderer* r);
    void generate_upload_commands_for_textures(vk::CommandBuffer upload_cmds);

    void texture_window_gui(bool* open);

    gpu_shared_value_heap<glm::mat4> transforms;

  public:
    scene_renderer(renderer* r, flecs::world& world, std::unique_ptr<render_pipeline> pipeline);
    void start_resource_upload(
        renderer* r, std::shared_ptr<asset_bundle> bundle, vk::CommandBuffer upload_cmds
    );
    void resource_upload_cleanup();
    void render_frame(frame& frame);
};
