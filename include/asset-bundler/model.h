#pragma once
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

using std::byte;
using std::filesystem::path;
using texture_id                 = uint16_t;
const texture_id INVALID_TEXTURE = 0;
using string_id                  = uint32_t;

struct texture_info {
    texture_info(string_id name, uint32_t width, uint32_t height, vk::Format format, stbi_uc* data, size_t len)
        : name(name), width(width), height(height), format(format), data(data), len(len) {}

    string_id  name;
    uint32_t   width, height;
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
            throw std::runtime_error("invalid number of channels to convert to Vulkan format: " + std::to_string(nchannels));
    }
}

bool texture_is_single_value(int w, int h, int channels, const stbi_uc* data);

struct vertex {
    aiVector3D position, normal, tangent;
    aiVector2D tex_coord;
};

using index_type = uint32_t;

struct mesh_info {
    size_t vertex_offset, index_offset, index_count, material_index;
};

struct material_info {
    texture_id base_color, normals, roughness, metallic;

    material_info()
        : base_color(INVALID_TEXTURE), normals(INVALID_TEXTURE), roughness(INVALID_TEXTURE), metallic(INVALID_TEXTURE) {}

    void set_texture(aiTextureType type, texture_id texture) {
#define X(T, N)                                                                                                                \
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
