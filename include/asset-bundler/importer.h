#pragma once
#include "asset-bundler/model.h"
#include "asset-bundler/output_bundle.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <stb_image.h>

class importer {
    Assimp::Importer           aimp;
    std::vector<path>          models;
    std::map<texture_id, path> textures;

    output_bundle& out;

    inline texture_id add_texture_path(std::filesystem::path p) {
        auto id = out.reserve_texture_id();
        textures.emplace(id, p);
        return id;
    }

    void load_mesh(const aiMesh* m, const aiScene* scene, size_t mat_index_offset);

    void load_model(const path& ip);

    void load_texture(texture_id id, const path& ip);

  public:
    importer(output_bundle& out, int argc, char* argv[]);

    void load();
};
