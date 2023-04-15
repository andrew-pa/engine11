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

    strings = (string_header*)header_ptr;
    header_ptr += sizeof(string_header) * header->num_strings;
    textures = (texture_header*)header_ptr;
    header_ptr += sizeof(texture_header) * header->num_textures;
    meshes = (mesh_header*)header_ptr;
    header_ptr += sizeof(mesh_header) * header->num_meshes;
    materials = (material_header*)header_ptr;
    header_ptr += sizeof(material_header) * header->num_materials;
    objects = (object_header*)header_ptr;
    header_ptr += sizeof(object_header) * header->num_objects;
    groups = (group_header*)header_ptr;
    header_ptr += sizeof(asset_bundle_format::group_header) * header->num_groups;
    assert(header_ptr == bundle_data + header->data_offset);
}

asset_bundle::~asset_bundle() { free(bundle_data); }

std::string_view asset_bundle::string(string_id id) const {
    auto* sh = strings + (id - 1);
    assert(sh->id == id);
    return {(char*)(bundle_data + sh->offset), sh->len};
}

const texture_header& asset_bundle::texture(texture_id id) const {
    auto* th = textures + (id - 1);
    assert(th->id == id);
    return *th;
}
