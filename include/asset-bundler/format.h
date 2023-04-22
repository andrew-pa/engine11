#pragma once
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

using std::byte;
using texture_id                 = uint16_t;
const texture_id INVALID_TEXTURE = 0;
using string_id                  = uint32_t;
using object_id                  = uint32_t;

namespace asset_bundle_format {
struct header {
    size_t num_strings, num_textures, num_materials, num_meshes, num_objects, num_groups,
        num_total_vertices, vertex_start_offset, num_total_indices, index_start_offset, data_offset,
        gpu_data_offset;
};

struct string_header {
    string_id id;
    size_t    offset, len;
};

struct texture_header {
    texture_id id;
    string_id  name;
    uint32_t   width, height;
    VkFormat   format;
    size_t     offset;
};

struct mesh_header {
    size_t vertex_offset, index_offset, index_count, material_index;
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
};

struct group_header {
    string_id name;
    uint32_t  num_objects;
    size_t    offset;
};
};  // namespace asset_bundle_format
