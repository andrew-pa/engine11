#pragma once
#include "asset-bundler/format.h"
#include <assimp/scene.h>
#include <filesystem>
#include <iostream>
#include <map>
#include <stb_image.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>

using std::filesystem::path;

struct texture_info {
    texture_info(
        string_id  name,
        uint32_t   width,
        uint32_t   height,
        vk::Format format,
        stbi_uc*   data
    )
        : name(name), width(width), height(height), format(format), data(data){}

    string_id  name;
    uint32_t   width, height, mip_levels;
    vk::Format format;
    stbi_uc*   data;
    size_t     len;
};

inline vk::Format format_from_channels(int nchannels) {
    switch(nchannels) {
        case 1: return vk::Format::eR8Unorm;
        case 2: return vk::Format::eR8G8Unorm;
        case 3: return vk::Format::eR8G8B8Unorm;
        case 4: return vk::Format::eR8G8B8A8Unorm;
        default:
            throw std::runtime_error(
                "invalid number of channels to convert to Vulkan format: "
                + std::to_string(nchannels)
            );
    }
}

// struct vertex {
//     aiVector3D position, normal, tangent;
//     aiVector2D tex_coord;
// };

// TODO these are duplicate of mesh_header and material_header
struct mesh_info {
    size_t vertex_offset, index_offset, index_count, material_index;
};

struct material_info {
    string_id  name;
    texture_id base_color, normals, roughness, metallic;

    material_info(string_id name)
        : name(name), base_color(INVALID_TEXTURE), normals(INVALID_TEXTURE),
          roughness(INVALID_TEXTURE), metallic(INVALID_TEXTURE) {}

    void set_texture(aiTextureType type, texture_id texture) {
#define X(T, N)                                                                                    \
    case T: N = texture; break;
        switch(type) {
            X(aiTextureType_DIFFUSE, base_color)
            X(aiTextureType_NORMALS, normals)
            X(aiTextureType_METALNESS, metallic)
            X(aiTextureType_SHININESS, roughness)
            default: throw std::runtime_error("unsupported texture type");
        }
#undef X
    }
};

struct object_info {
    string_id             name;
    std::vector<uint32_t> mesh_indices;
    aiMatrix4x4           transform;
};

struct group_info {
    string_id              name;
    std::vector<object_id> objects;
};
