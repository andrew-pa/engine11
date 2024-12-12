#pragma once
#include "egg/components.h"
#include "egg/renderer/core/frame_renderer.h"
#include "egg/renderer/memory.h"
#include "egg/renderer/renderer.h"
#include "glm.h"

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

    texture(
        renderer*                         r,
        const asset_bundle_format::image& img_info,
        vk::ImageViewType                 type,
        vk::ImageCreateFlags              flags = {}
    );
};

struct environment {
    texture sky, diffuse_irradiance;
};

struct per_object_push_constants {
    uint32_t   transform_index;
    texture_id base_color, normals, roughness, metallic;
};

struct shader_uniform_values {
    vec3 camera_pos;
};

struct gpu_static_scene_data {
    gpu_static_scene_data(
        renderer*                            r,
        const std::shared_ptr<asset_bundle>& bundle,
        vk::CommandBuffer                    upload_cmds,
        const renderer_features&             features
    );

    std::unique_ptr<gpu_buffer> vertex_buffer, index_buffer, staging_buffer;
    std::unique_ptr<gpu_buffer> cube_vertex_buffer, cube_index_buffer;
    // TODO: these texture maps are dubious, maybe we should make the linear ordering of texture ids
    // explicit so things are faster
    std::unordered_map<texture_id, texture>    textures;
    std::unordered_map<string_id, environment> envs;
    string_id                                  current_env;

    vk::UniqueSampler             texture_sampler;
    vk::UniqueDescriptorSetLayout desc_set_layout;
    vk::UniqueDescriptorPool      desc_set_pool;
    vk::DescriptorSet             desc_set;  // lifetime is tied to pool

    std::unique_ptr<gpu_buffer>                     object_accel_struct_storage, accel_scratch;
    std::vector<vk::UniqueAccelerationStructureKHR> object_accel_structs;

    std::vector<vk::DescriptorImageInfo> setup_descriptors(
        renderer* r, asset_bundle* bundle, std::vector<vk::WriteDescriptorSet>& writes
    );
    void resource_upload_cleanup();

  private:
    void load_geometry_from_bundle(
        renderer*                r,
        asset_bundle*            current_bundle,
        vk::CommandBuffer        upload_cmds,
        const renderer_features& features
    );
    void create_textures_from_bundle(renderer* r, asset_bundle* current_bundle);
    void create_envs_from_bundle(renderer* r, asset_bundle* current_bundle);
    void generate_upload_commands_for_texture(
        asset_bundle*                     current_bundle,
        vk::CommandBuffer                 upload_cmds,
        const texture&                    t,
        const asset_bundle_format::image& img,
        size_t                            bundle_offset
    ) const;
    void generate_upload_commands_for_textures(
        asset_bundle* current_bundle, vk::CommandBuffer upload_cmds
    );
    void generate_upload_commands_for_envs(
        asset_bundle* current_bundle, vk::CommandBuffer upload_cmds
    );
    void build_object_accel_structs(
        renderer* r, asset_bundle* current_bundle, vk::CommandBuffer upload_cmds
    );

    void texture_window_gui(bool* open, std::shared_ptr<asset_bundle> current_bundle);
};

struct scene_raytracing {
    std::unique_ptr<gpu_buffer> tlas_storage, scratch_buffer;
    size_t tlas_storage_size{}, scratch_buffer_size{};
    vk::UniqueAccelerationStructureKHR tlas;

    scene_raytracing()
        : tlas_storage(nullptr), tlas(VK_NULL_HANDLE) {}

    void build(class scene_renderer* sr, vk::CommandBuffer cmd_buf);
};

class scene_renderer {
    renderer*                     r;
    std::shared_ptr<flecs::world> world;
    std::shared_ptr<asset_bundle> current_bundle;

    std::unordered_set<vk::Format> supported_depth_formats;
    vk::AttachmentDescription      surface_color_attachment;
    rendering_algorithm*           algo;
    renderer_features              features;

    std::unique_ptr<gpu_static_scene_data> scene_data;

    gpu_shared_value_heap<glm::mat4>        transforms;
    gpu_shared_value<shader_uniform_values> shader_uniforms;
    gpu_shared_value_heap<light_info>       gpu_lights;

    std::optional<scene_raytracing> rt_state;

    vk::UniqueCommandBuffer scene_render_cmd_buffer;
    bool                    should_regenerate_command_buffer;

    void generate_scene_draw_commands(vk::CommandBuffer cb, vk::PipelineLayout pl);

    std::vector<flecs::observer>                                        observers;
    flecs::query<tag::active_camera, comp::gpu_transform, comp::camera> active_camera_q;
    flecs::query<comp::gpu_transform, comp::renderable>                 renderable_q;
    void                                                                setup_ecs();

    friend struct scene_raytracing;
  public:
    scene_renderer(renderer* r, std::shared_ptr<flecs::world> world, rendering_algorithm* algo);

    ~scene_renderer();

    rendering_algorithm* swap_rendering_algorithm(rendering_algorithm* new_algo);

    void start_resource_upload(
        const std::shared_ptr<asset_bundle>& bundle, vk::CommandBuffer upload_cmds
    );
    void setup_scene_post_upload();
    void resource_upload_cleanup();

    void create_swapchain_depd(frame_renderer* fr);

    void render_frame(frame& frame);
};
