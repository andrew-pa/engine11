#include "egg/bundle.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>

using asset_bundle_format::group_header;
using asset_bundle_format::header;
using asset_bundle_format::material_header;
using asset_bundle_format::mesh_header;
using asset_bundle_format::object_header;
using asset_bundle_format::string_header;
using asset_bundle_format::texture_header;

asset_bundle::asset_bundle(const std::filesystem::path& location) {
    std::cout << "loading bundle from " << location << "...";
    std::cout.flush();
    std::ifstream file(location, std::ios::ate | std::ios::binary);
    if(!file)
        throw std::runtime_error(std::string("failed to load bundle file at: ") + location.c_str());
    size_t compressed_buffer_size = (size_t)file.tellg();
    byte*  compressed_buffer      = (byte*)malloc(compressed_buffer_size);
    file.seekg(0);
    file.read((char*)compressed_buffer, compressed_buffer_size);
    file.close();
    std::cout << " read " << compressed_buffer_size << " bytes\n";

    std::cout << "decompressing bundle... ";
    std::cout.flush();
    size_t size = ZSTD_decompressBound(compressed_buffer, compressed_buffer_size);
    bundle_data = (byte*)malloc(size);
    total_size  = ZSTD_decompress(bundle_data, size, compressed_buffer, compressed_buffer_size);
    std::cout << " got " << total_size << " bytes\n";
    free(compressed_buffer);

    header           = (struct header*)bundle_data;
    byte* header_ptr = bundle_data + sizeof(asset_bundle_format::header);

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
    assert(header_ptr == bundle_data + header->data_offset);

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

void asset_bundle::take_gpu_data(byte* dest) {
    assert(!gpu_data_taken);
    memcpy(dest, bundle_data + header->gpu_data_offset, gpu_data_size());
    bundle_data    = (byte*)realloc(bundle_data, header->gpu_data_offset);
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
