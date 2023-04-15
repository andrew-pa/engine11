#pragma once
#include "asset-bundler/model.h"
#include <cstddef>
#include <cstdint>
#include <vulkan/vulkan.h>

namespace asset_bundle_format {
struct header {
    size_t num_strings, num_textures, num_materials, num_meshes, num_objects, num_groups, num_total_vertices,
        vertex_start_offset, num_total_indices, index_start_offset;
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

using mesh_header     = mesh_info;
using material_header = material_info;

struct object_header {
    string_id name;
    uint32_t  num_meshes;
    size_t    offset;
    float     transform_matrix[16];
};

struct group_header {
    string_id name;
    uint32_t  num_objects;
    size_t    offset;
};
};  // namespace asset_bundle_format
