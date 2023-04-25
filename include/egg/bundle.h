#pragma once
#include "asset-bundler/format.h"
#include <filesystem>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

class asset_bundle {
    // std::unordered_map<string_id, std::string> strings;
    // std::unordered_map<texture_id, asset_bundle_format::texture_header> textures;
    // std::vector<asset_bundle_format::mesh_header> meshes;
    // std::vector<asset_bundle_format::material_header> materials;
    // std::vector<asset_bundle_format::object_header> objects;
    // std::vector<asset_bundle_format::group_header> groups;
    byte*  bundle_data    = nullptr;
    size_t total_size     = 0;
    bool   gpu_data_taken = false;

    asset_bundle_format::header*          header;
    asset_bundle_format::string_header*   strings;
    asset_bundle_format::texture_header*  textures;
    asset_bundle_format::mesh_header*     meshes;
    asset_bundle_format::material_header* materials;
    asset_bundle_format::object_header*   objects;
    asset_bundle_format::group_header*    groups;

  public:
    asset_bundle(const std::filesystem::path& location);
    ~asset_bundle();

    inline const asset_bundle_format::header& bundle_header() const { return *header; }

    // dest must be big enough to store all GPU data
    void take_gpu_data(byte* dest);

    inline size_t gpu_data_size() const { return total_size - bundle_header().gpu_data_offset; }

    std::string_view                           string(string_id id) const;
    const asset_bundle_format::texture_header& texture(texture_id id) const;
    const asset_bundle_format::texture_header& texture_by_index(size_t i) const;

    const glm::mat4&                            object_transform(object_id id) const;
    class object_mesh_iterator                  object_meshes(object_id id) const;
    string_id                                   object_name(object_id id) const;
    const asset_bundle_format::material_header& material(size_t index) const;

    class group_object_iterator group_objects(size_t group_index) const;
};

class object_mesh_iterator {
    asset_bundle_format::mesh_header* meshes;
    uint32_t*                         indices;
    size_t                            count;

    object_mesh_iterator(asset_bundle_format::mesh_header* meshes, uint32_t* indices, size_t count)
        : meshes(meshes), indices(indices), count(count) {}

  public:
    const asset_bundle_format::mesh_header& operator*() { return meshes[*indices]; }

    const asset_bundle_format::mesh_header* operator->() { return &meshes[*indices]; }

    void operator++() {
        assert(count > 0);
        indices++;
        count--;
    }

    bool has_more() const { return count > 0; }

    friend class asset_bundle;
};

class group_object_iterator {
    object_id* indices;
    size_t     count;

    group_object_iterator(object_id* indices, size_t count) : indices(indices), count(count) {}

  public:
    object_id operator*() { return *indices; }

    object_id* operator->() { return indices; }

    void operator++() {
        assert(count > 0);
        indices++;
        count--;
    }

    bool has_more() const { return count > 0; }

    friend class asset_bundle;
};
