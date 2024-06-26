#pragma once
#include "glm.h"
#include <cstddef>
#include <cstdint>
#include <vulkan/vulkan.hpp>

using texture_id                 = uint16_t;
const texture_id INVALID_TEXTURE = 0;
using string_id                  = uint32_t;
using object_id                  = uint32_t;

struct vertex {
    vec3 position, normal, tangent;
    vec2 tex_coord;
};

constexpr static vk::VertexInputAttributeDescription vertex_attribute_description[]{
    {0, 0, vk::Format::eR32G32B32Sfloat, offsetof(vertex, position) },
    {1, 0, vk::Format::eR32G32B32Sfloat, offsetof(vertex, normal)   },
    {2, 0, vk::Format::eR32G32B32Sfloat, offsetof(vertex, tangent)  },
    {3, 0, vk::Format::eR32G32Sfloat,    offsetof(vertex, tex_coord)},
};

using index_type = uint32_t;

namespace asset_bundle_format {
struct header {
    size_t num_strings, num_textures, num_materials, num_meshes, num_objects, num_groups,
        num_environments, num_total_vertices, vertex_start_offset, num_total_indices,
        index_start_offset, data_offset, gpu_data_offset;
};

struct string_header {
    string_id id;
    size_t    offset, len;
};

struct image {
    uint32_t width, height, mip_levels, array_layers;
    VkFormat format;
};

struct texture_header {
    texture_id id;
    string_id  name;
    image      img;
    size_t     offset;
};

struct environment_header {
    string_id name;
    image     skybox;
    size_t    skybox_offset;
    image     diffuse_irradiance;
    size_t    diffuse_irradiance_offset;
};

struct mesh_header {
    size_t vertex_offset, index_offset, index_count, material_index;
    aabb   bounds;
};

struct material_header {
    string_id  name;
    texture_id base_color, normals, roughness, metallic;

    material_header(string_id name)
        : name(name), base_color(INVALID_TEXTURE), normals(INVALID_TEXTURE),
          roughness(INVALID_TEXTURE), metallic(INVALID_TEXTURE) {}
};

struct object_header {
    string_id name;
    uint32_t  num_meshes;
    size_t    offset;
    glm::mat4 transform_matrix;
    aabb      bounds;
};

struct group_header {
    string_id name;
    uint32_t  num_objects;
    size_t    offset;
    aabb      bounds;
};
};  // namespace asset_bundle_format
