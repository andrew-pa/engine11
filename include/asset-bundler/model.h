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

struct image_info {
    uint32_t width, height, mip_levels, array_layers;
    vk::Format format;

    inline vk::ImageCreateInfo vulkan_create_info(vk::ImageUsageFlags usage, vk::ImageCreateFlags flags = {}) const {
        return vk::ImageCreateInfo{
            flags,
            vk::ImageType::e2D,
            format,
            vk::Extent3D{width, height, 1},
            mip_levels,
            array_layers,
            vk::SampleCountFlagBits::e1,
            vk::ImageTiling::eOptimal,
            usage
        };
    }

    inline vk::ImageViewCreateInfo vulkan_full_image_view(vk::Image img, vk::ImageViewType type) const {
        return vk::ImageViewCreateInfo {
            {},
            img,
            type,
            format,
            vk::ComponentMapping{},
            vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, mip_levels, 0, array_layers}
        };
    }

    inline asset_bundle_format::image as_image() const {
        return asset_bundle_format::image{
            .width = width,
            .height = height,
            .mip_levels = mip_levels,
            .array_layers = array_layers,
            .format = (VkFormat)format
        };
    }
};

struct texture_info {
    texture_info(
        string_id  name,
        uint32_t   width,
        uint32_t   height,
        vk::Format format,
        stbi_uc*   data
    )
        : name(name), img{ .width = width, .height = height, .mip_levels = 0, .array_layers = 1, .format = format }, data(data){}

    string_id  name;
    image_info img;
    // TODO: we can remove this since now it is on the GPU
    stbi_uc*   data;
    size_t     len;
};

struct environment_info {
    // string id of name is used to identify environment job
    string_id name;

    // skybox cubemap
    image_info skybox;
    // diffuse irradiance cubemap
    image_info diffuse_irradiance;
    size_t diffuse_irradiance_offset;

    // total length of all data
    size_t len;
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

struct options {
    bool enable_ibl_precomputation = false;
};
