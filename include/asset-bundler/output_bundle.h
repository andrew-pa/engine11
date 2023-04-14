#pragma once
#include "asset-bundler/model.h"

class output_bundle {
    path                                       output_path;
    string_id                                  next_string_id = 1;
    std::unordered_map<string_id, std::string> strings;
    texture_id                                 next_texture_id = 1;
    std::map<texture_id, texture_info>         textures;
    std::vector<material_info>                 materials;

    std::vector<vertex>     vertices;
    std::vector<index_type> indices;

  public:
    output_bundle(path output_path) : output_path(std::move(output_path)) {}

    string_id add_string(std::string s) {
        auto id = next_string_id++;
        strings.emplace(id, s);
        return id;
    }

    texture_id reserve_texture_id() { return next_texture_id++; }

    void add_texture(texture_id id, std::string name, uint32_t width, uint32_t height, int nchannels, stbi_uc* data);

    // returns the current vertex offset
    size_t start_vertex_gather(size_t num_verts) {
        vertices.reserve(num_verts);
        return vertices.size();
    }

    // returns the current index offset
    size_t start_index_gather(size_t num_idx) {
        indices.reserve(num_idx);
        return indices.size();
    }

    inline void add_vertex(vertex&& v) { vertices.emplace_back(v); }

    inline void add_index(index_type i) { indices.emplace_back(i); }

    void add_mesh(mesh_info&& info) {
        // TODO
    }

    inline size_t num_materials() { return materials.size(); }

    void add_material(material_info&& mat) { materials.emplace_back(mat); }

    void write();

    ~output_bundle();
};
