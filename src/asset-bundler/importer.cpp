#include "asset-bundler/importer.h"
#include "asset-bundler/format.h"
#include "asset-bundler/texture_processor.h"
#include "fs-shim.h"
#include "glm/common.hpp"
#include <limits>

const std::unordered_set<std::string> texture_exts     = {".png", ".jpg", ".bmp"};
const std::unordered_set<std::string> environment_exts = {".hdr"};

importer::importer(output_bundle& out, const std::vector<std::filesystem::path>& input_paths)
    : out(out) {
    for(const auto& input : input_paths) {
        auto ext = path_to_string(input.extension());
        if(aimp.IsExtensionSupported(ext.c_str()))
            models.emplace_back(input);
        else if(texture_exts.find(ext) != texture_exts.end())
            add_texture_path(input);
        else if(environment_exts.find(ext) != environment_exts.end())
            environments.emplace_back(input);
        else
            std::cout << "unknown file type: " << input.extension() << "\n";
    }
}

glm::vec3 from_a(const aiVector3D& a) { return {a.x, a.y, a.z}; }

mat4 from_a(const aiMatrix4x4& t) {
    // assimp uses row major matrices but GLSL/the engine is column major
    return {
        vec4(t.a1, t.b1, t.c1, t.d1),
        vec4(t.a2, t.b2, t.c2, t.d2),
        vec4(t.a3, t.b3, t.c3, t.d3),
        vec4(t.a4, t.b4, t.c4, t.d4)
    };
}

inline aabb aabb_from_ai(const aiAABB& bb) {
    return aabb{
        .min = from_a(bb.mMin),
        .max = from_a(bb.mMax),
    };
}

void importer::load_mesh(const aiMesh* m, const aiScene* scene, size_t mat_index_offset) {
    std::cout << "\t\t " << m->mName.C_Str() << " ("
              << scene->mMaterials[m->mMaterialIndex]->GetName().C_Str() << ") " << m->mNumVertices
              << " vertices, " << m->mNumFaces << " faces"
              << " \n";
    size_t vertex_offset = out.start_vertex_gather(m->mNumVertices);
    for(size_t i = 0; i < m->mNumVertices; ++i) {
        out.add_vertex(vertex{
            .position  = from_a(m->mVertices[i]),
            .normal    = from_a(m->mNormals[i]),
            .tangent   = from_a(m->mTangents[i]),
            .tex_coord = glm::vec2(m->mTextureCoords[0][i].x, m->mTextureCoords[0][i].y)
        });
    }
    auto   index_count  = m->mNumFaces * 3;
    size_t index_offset = out.start_index_gather(index_count);
    for(size_t f = 0; f < m->mNumFaces; ++f) {
        assert(m->mFaces[f].mNumIndices == 3);
        for(int i = 0; i < 3; ++i)
            out.add_index(m->mFaces[f].mIndices[i]);
    }
    out.add_mesh(mesh_info{
        .vertex_offset  = vertex_offset,
        .index_offset   = index_offset,
        .index_count    = index_count,
        .material_index = m->mMaterialIndex + mat_index_offset,
        .bounds         = aabb_from_ai(m->mAABB)
    });
}

// only support two levels: groups and objects? makes scene graphs simpler but ECS probably won't be
// a scene graph itself????
void importer::load_graph(const aiNode* node, aiMesh** const meshInfos) {
    for(size_t i = 0; i < node->mNumChildren; ++i) {
        auto* c = node->mChildren[i];
        if(c->mNumMeshes > 0 && c->mNumChildren == 0)
            load_object(c, meshInfos);
        else
            load_group(c, meshInfos);
    }
}

void importer::load_group(const aiNode* node, aiMesh** const meshInfos) {
    std::cout << "\t\t\t group: " << node->mName.C_Str() << "\n";
    std::vector<object_id> members;
    members.reserve(node->mNumChildren);
    aabb bounds{vec3(std::numeric_limits<float>::max()), vec3(std::numeric_limits<float>::min())};
    for(size_t i = 0; i < node->mNumChildren; ++i) {
        auto* c = node->mChildren[i];
        if(c->mNumMeshes > 0 && c->mNumChildren == 0) {
            auto [id, bb] = load_object(c, meshInfos);
            bounds.extend(bb);
            members.emplace_back(id);
        } else {
            std::cout << "\t\t\t\t invalid subgroup " << c->mNumChildren << "\n";
        }
    }
    out.add_group(group_info{
        .name = out.add_string(node->mName.C_Str()), .objects = members, .bounds = bounds
    });
}

std::pair<object_id, aabb> importer::load_object(const aiNode* node, aiMesh** const meshInfos) {
    std::cout << "\t\t\t\t object: " << node->mName.C_Str() << " " << node->mNumMeshes
              << " meshes \n";
    // if(!node->mTransformation.IsIdentity()) std::cout << "\t\t\t\t\t transform != identity\n";
    std::vector<uint32_t> meshes;
    meshes.reserve(node->mNumMeshes);
    vec3 bound_min = vec3(std::numeric_limits<float>::max()),
         bound_max = vec3(std::numeric_limits<float>::min());
    for(size_t i = 0; i < node->mNumMeshes; ++i) {
        // !!! Assume that we load meshes after we load the graph
        meshes.emplace_back(node->mMeshes[i] + out.num_meshes());
        const auto& b = meshInfos[node->mMeshes[i]]->mAABB;
        bound_min     = glm::min(bound_min, from_a(b.mMin));
        bound_max     = glm::max(bound_max, from_a(b.mMax));
    }
    aabb bounds{bound_min, bound_max};
    mat4 t = from_a(node->mTransformation);
    return {
        out.add_object(object_info{
            .name         = out.add_string(node->mName.C_Str()),
            .mesh_indices = meshes,
            .transform    = t,
            .bounds       = bounds
        }),
        bounds.transformed(t)
    };
}

path path_from_assimp(const aiString& tpath) {
    std::string tp{tpath.C_Str()};
    for(char& i : tp)
        if(i == '\\') i = '/';
    return tp;
}

void importer::load_model(const path& ip) {
    std::cout << "\t" << ip << "\n";
    // TODO: why does aiProcessPreset_TargetRealtime_MaxQuality seg fault because it doesn't
    // generate tangents??
    const aiScene* scene = aimp.ReadFile(
        path_to_string(ip),
        aiProcessPreset_TargetRealtime_Fast | aiProcess_FlipUVs | aiProcess_GenBoundingBoxes
    );
    std::cout << "\t\t" << scene->mNumMeshes << " meshes, " << scene->mNumMaterials
              << " materials\n";

    load_graph(scene->mRootNode, scene->mMeshes);

    size_t start_mat_index = out.num_materials();
    for(size_t i = 0; i < scene->mNumMeshes; ++i)
        load_mesh(scene->mMeshes[i], scene, start_mat_index);

    for(size_t i = 0; i < scene->mNumMaterials; ++i) {
        const auto*   mat = scene->mMaterials[i];
        material_info info{out.add_string(mat->GetName().C_Str())};
        // TODO: read opacity map if present
        // TODO: convert all RGB8 textures to RGBA8 textures, using opacity map if possible
        aiString tpath;
        if(mat->GetTexture(aiTextureType_DIFFUSE, 0, &tpath) == aiReturn_SUCCESS) {
            auto diffuse_path = path_from_assimp(tpath);
            auto tid          = add_texture_path(ip.parent_path() / diffuse_path);
            info.set_texture(aiTextureType_DIFFUSE, tid);
            if(mat->GetTexture(aiTextureType_OPACITY, 0, &tpath) == aiReturn_SUCCESS) {
                std::cout << "texture has opacity @ " << tpath.C_Str() << "\n";
                auto opacity_path = path_from_assimp(tpath);
                // Blender seems to write the DIFFUSE texture in to this slot if it has an alpha
                // channel, which is redundant
                if(diffuse_path != opacity_path)
                    std::get<1>(textures[tid]) = ip.parent_path() / opacity_path;
            }
        }
        for(auto tt : {aiTextureType_NORMALS, aiTextureType_METALNESS, aiTextureType_SHININESS})
            if(mat->GetTexture(tt, 0, &tpath) == aiReturn_SUCCESS)
                info.set_texture(tt, add_texture_path(ip.parent_path() / path_from_assimp(tpath)));
        out.add_material(std::move(info));
    }
}

bool texture_is_single_value(int w, int h, int channels, const stbi_uc* data) {
    assert(channels <= 4);
    for(int y = 0; y < h; ++y) {
        for(int x = 0; x < w; ++x) {
            for(int c = 0; c < channels; ++c)
                if(data[c] != data[c + x * channels + y * w * channels]) return false;
        }
    }
    return true;
}

stbi_uc* insert_alpha_channel_into_rgb8(int width, int height, stbi_uc* data, stbi_uc* alpha) {
    stbi_uc* new_data = (stbi_uc*)malloc(width * height * 4);
    for(int y = 0; y < height; ++y) {
        for(int x = 0; x < width; ++x) {
            for(int c = 0; c < 3; ++c)
                new_data[c + x * 4 + y * width * 4] = data[c + x * 3 + y * width * 3];
            new_data[3 + x * 4 + y * width * 4] = alpha == nullptr ? 0xff : alpha[x + y * width];
        }
    }
    return new_data;
}

void importer::load_texture(texture_id id, const std::tuple<path, std::optional<path>>& ip) {
    const auto& [main_texture_path, opacity_texture_path] = ip;
    std::cout << "\t" << main_texture_path << " (" << id << ") \n";
    int   width, height, channels;
    auto* data = stbi_load(
        path_to_string(main_texture_path).c_str(), &width, &height, &channels, STBI_default
    );
    if(data == nullptr) {
        std::cout << "\t\tfailed to load texture " << main_texture_path << ": "
                  << stbi_failure_reason() << "\n";
        return;
    }
    stbi_uc* opacity_data = nullptr;
    if(opacity_texture_path.has_value()) {
        if(channels != 3) {
            std::cout << "warning: loading opacity map (" << opacity_texture_path.value()
                      << ") for texture with " << channels << " channels, which is unsupported\n";
        }
        int ax, ay;
        opacity_data = stbi_load(
            path_to_string(opacity_texture_path.value()).c_str(), &ax, &ay, nullptr, STBI_grey
        );
        if(opacity_data == nullptr) {
            std::cout << "\t\tfailed to load texture " << opacity_texture_path.value() << ": "
                      << stbi_failure_reason() << "\n";
            return;
        }
        if(ax != width || ay != height) {
            std::cout << "warning: expected opacity map (" << opacity_texture_path.value()
                      << ") to be the same size as base texture" << ax << "x" << ay << " vs "
                      << width << "x" << height << "\n";
            free(opacity_data);
            opacity_data = nullptr;
        }
    }
    std::cout << "\t\tloaded texture " << width << "x" << height << " c=" << channels << "\n";
    // check if the texture is silly and we can store it as a single texel
    // the output stage will truncate the data when it copies it into the file
    if(texture_is_single_value(width, height, channels, data)
       && (opacity_data == nullptr || texture_is_single_value(width, height, 1, opacity_data))) {
        width  = 1;
        height = 1;
    }
    if(channels == 3) {
        auto* new_data = insert_alpha_channel_into_rgb8(width, height, data, opacity_data);
        free(data);
        free(opacity_data);
        data     = new_data;
        channels = 4;
    }
    // TODO: possibly we could also apply compression with stb_dxt and save more VRAM
    out.add_texture(
        id, path_to_string(main_texture_path.filename()), width, height, channels, data
    );
}

void importer::load_env(const path& ip) {
    std::cout << "\t" << ip << "\n";
    int   width, height, channels;
    auto* data = stbi_loadf(path_to_string(ip).c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if(data == nullptr) {
        std::cout << "\t\tfailed to load environment map " << ip << ": " << stbi_failure_reason()
                  << "\n";
        return;
    }
    out.add_environment(path_to_string(ip.filename()), width, height, 4, data);
}

void importer::load() {
    std::cout << "loading models:\n";
    for(const auto& ip : models)
        load_model(ip);

    std::cout << "loading textures:\n";
    for(const auto& [id, ip] : textures)
        load_texture(id, ip);

    std::cout << "loading environments:\n";
    for(const auto& ip : environments)
        load_env(ip);
}
