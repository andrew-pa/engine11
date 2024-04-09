#include "egg/bundle.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#include <fs-shim.h>
#include <chrono>

using asset_bundle_format::group_header;
using asset_bundle_format::header;
using asset_bundle_format::material_header;
using asset_bundle_format::mesh_header;
using asset_bundle_format::object_header;
using asset_bundle_format::string_header;
using asset_bundle_format::texture_header;
using asset_bundle_format::environment_header;

asset_bundle::asset_bundle(const std::filesystem::path& location) {
    std::cout << "loading bundle from " << location << "...";
    auto start_read_time = std::chrono::system_clock::now();
    std::cout.flush();
    std::ifstream file(location, std::ios::ate | std::ios::binary);
    if(!file)
        throw std::runtime_error(std::string("failed to load bundle file at: ") + path_to_string(location));
    size_t compressed_buffer_size = (size_t)file.tellg();
    uint8_t*  compressed_buffer      = (uint8_t*)malloc(compressed_buffer_size);
    file.seekg(0);
    file.read((char*)compressed_buffer, compressed_buffer_size);
    file.close();
    std::cout << " read " << compressed_buffer_size << " bytes\n";
    auto end_read_time = std::chrono::system_clock::now();
    std::cout << "reading bundle took "
        << std::chrono::duration_cast<std::chrono::milliseconds>(end_read_time - start_read_time).count()
        << "\n";

    std::cout << "decompressing bundle... ";
    std::cout.flush();
    auto start_decom_time = std::chrono::system_clock::now();
    size_t size = ZSTD_decompressBound(compressed_buffer, compressed_buffer_size);
    bundle_data = (uint8_t*)malloc(size);
    total_size  = ZSTD_decompress(bundle_data, size, compressed_buffer, compressed_buffer_size);
    std::cout << " got " << total_size << " bytes\n";
    free(compressed_buffer);
    auto end_decom_time = std::chrono::system_clock::now();
    std::cout << "decompressing bundle took "
        << std::chrono::duration_cast<std::chrono::milliseconds>(end_decom_time-start_decom_time).count()
        << "\n";

    header           = (struct header*)bundle_data;
    uint8_t* header_ptr = bundle_data + sizeof(asset_bundle_format::header);

    // important that these are in the same order as in output_bundle::write()'s copy_X() calls
    strings = (string_header*)header_ptr;
    header_ptr += sizeof(string_header) * header->num_strings;
    materials = (material_header*)header_ptr;
    header_ptr += sizeof(material_header) * header->num_materials;
    meshes = (mesh_header*)header_ptr;
    header_ptr += sizeof(mesh_header) * header->num_meshes;
    objects = (object_header*)header_ptr;
    header_ptr += sizeof(object_header) * header->num_objects;
    groups = (group_header*)header_ptr;
    header_ptr += sizeof(asset_bundle_format::group_header) * header->num_groups;
    textures = (texture_header*)header_ptr;
    header_ptr += sizeof(texture_header) * header->num_textures;
    environments = (environment_header*)header_ptr;
    header_ptr += sizeof(environment_header) * header->num_environments;

    std::cout << "bundle CPU data " << header->gpu_data_offset << " bytes, "
              << " GPU data " << gpu_data_size() << " bytes\n";

    /*for(size_t i = 0; i < header->num_strings; ++i) {
        std::cout << "string " << i
            << "/" << strings[i].id
            << " " << strings[i].offset
            << ":" << strings[i].len << "\n";
    }

    for(size_t i = 0; i < header->num_textures; ++i) {
        const auto& th = textures[i];
        std::cout << "texture "
            << " " << th.id << "|" << th.name << "|" << th.offset
            << " " << th.width << "x" << th.height
            << " " << (uint32_t)(th.format) << "\n";
    }*/
}

asset_bundle::~asset_bundle() { free(bundle_data); }

void asset_bundle::take_gpu_data(uint8_t* dest) {
    assert(!gpu_data_taken);
    memcpy(dest, bundle_data + header->gpu_data_offset, gpu_data_size());
    bundle_data    = (uint8_t*)realloc(bundle_data, header->gpu_data_offset);
    gpu_data_taken = true;
}

std::string_view asset_bundle::string(string_id id) const {
    auto* sh = strings + (id - 1);
    // std::cout << "string " << id
    //         << "/" << sh->id
    //         << " " << sh->offset
    //         << ":" << sh->len << "\n";

    assert(sh->id == id);
    return {(char*)(bundle_data + sh->offset), sh->len};
}

const texture_header& asset_bundle::texture(texture_id id) const {
    auto* th = textures + (id - 1);
    assert(th->id == id);
    return *th;
}

const texture_header& asset_bundle::texture_by_index(size_t i) const { return *(textures + i); }
const environment_header& asset_bundle::environment_by_index(size_t i) const { return *(environments + i); }

#include <glm/gtx/io.hpp>
const glm::mat4& asset_bundle::object_transform(object_id id) const {
    // std::cout << "static transform for " << id << ": " << objects[id].transform_matrix << "\n";
    return objects[id].transform_matrix;
}

object_mesh_iterator asset_bundle::object_meshes(object_id id) const {
    return object_mesh_iterator{
        meshes, (uint32_t*)(bundle_data + objects[id].offset), objects[id].num_meshes};
}

string_id asset_bundle::object_name(object_id id) const { return objects[id].name; }

const aabb& asset_bundle::object_bounds(object_id id) const {
    return objects[id].bounds;
}

const asset_bundle_format::material_header& asset_bundle::material(size_t index) const {
    return materials[index];
}

string_id asset_bundle::group_name(size_t group_index) const { return groups[group_index].name; }

const aabb& asset_bundle::group_bounds(size_t group_index) const { return groups[group_index].bounds; }

group_object_iterator asset_bundle::group_objects(size_t group_index) const {
    return group_object_iterator{
        (uint32_t*)(bundle_data + groups[group_index].offset), groups[group_index].num_objects};
}

std::optional<size_t> asset_bundle::group_by_name(std::string_view name) const {
    for(size_t i = 0; i < header->num_groups; ++i) {
        if(string(groups[i].name) == name) return i;
    }
    return std::nullopt;
}
