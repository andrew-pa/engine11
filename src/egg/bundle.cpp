#include "egg/bundle.h"
#include <cassert>
#include <iostream>

using asset_bundle_format::group_header;
using asset_bundle_format::header;
using asset_bundle_format::material_header;
using asset_bundle_format::mesh_header;
using asset_bundle_format::object_header;
using asset_bundle_format::string_header;
using asset_bundle_format::texture_header;

asset_bundle::asset_bundle(byte* bundle_data) : bundle_data(bundle_data) {
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
