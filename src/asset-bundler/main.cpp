#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
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

/* asset-bundler:
 *  content pipeline & bundling utility
 *  roles:
 *      - load assets (3D models [meshes, materials, textures by reference], textures)
 *      - represent them in a uniform way
 *      - bundle them so they can be loaded quickly
 *  usage:
 *      asset-bundler <output bundle name> <input assets>...
 */

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

vk::Format format_from_channels(int nchannels) {
    switch(nchannels) {
        case 1: return vk::Format::eR8Unorm;
        case 2: return vk::Format::eR8G8Unorm;
        case 3: return vk::Format::eR8G8B8Unorm;
        case 4: return vk::Format::eR8G8B8A8Unorm;
        default:
            throw std::runtime_error("invalid number of channels to convert to Vulkan format: " + std::to_string(nchannels));
    }
}

bool texture_is_single_value(int w, int h, int channels, const stbi_uc* data) {
    assert(channels <= 4);
    for(int y = 0; y < h; ++y) {
        for(int x = 0; x < w; ++x) {
            for(int c = 0; c < channels; ++c)
                if(data[c] != data[c + x * channels + y * w]) return false;
        }
    }
    return true;
}

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

    void add_texture(texture_id id, std::string name, uint32_t width, uint32_t height, int nchannels, stbi_uc* data) {
        string_id ns = add_string(std::move(name));
        // check if the texture is silly and we can store it as a single texel
        // the output stage will truncate the data when it copies it into the file
        if(texture_is_single_value(width, height, nchannels, data)) {
            width  = 1;
            height = 1;
        }
        // TODO: we should probably also compute mipmaps
        textures.emplace(
            id, texture_info{ns, width, height, format_from_channels(nchannels), data, width * height * nchannels}
        );
    }

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

    void write() {}

    ~output_bundle() {
        for(const auto& [id, ifo] : textures)
            stbi_image_free(ifo.data);
    }
};

struct importer {
    Assimp::Importer           aimp;
    std::vector<path>          models;
    std::map<texture_id, path> textures;

    output_bundle& out;

    importer(output_bundle& out) : out(out) {}

    texture_id add_texture_path(std::filesystem::path p) {
        auto id = out.reserve_texture_id();
        textures.emplace(id, p);
        return id;
    }

    void load_mesh(const aiMesh* m, const aiScene* scene, size_t mat_index_offset) {
        std::cout << "\t\t " << m->mName.C_Str() << " (" << scene->mMaterials[m->mMaterialIndex]->GetName().C_Str() << ") "
                  << m->mNumVertices << " vertices, " << m->mNumFaces << " faces"
                  << " \n";
        size_t vertex_offset = out.start_vertex_gather(m->mNumVertices);
        for(size_t i = 0; i < m->mNumVertices; ++i) {
            out.add_vertex(vertex{
                .position  = m->mVertices[i],
                .normal    = m->mNormals[i],
                .tangent   = m->mTangents[i],
                .tex_coord = aiVector2D(m->mTextureCoords[0][i].x, m->mTextureCoords[0][i].y)});
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
            .material_index = m->mMaterialIndex + mat_index_offset});
    }

    void load_model(const path& ip) {
        std::cout << "\t" << ip << "\n";
        const aiScene* scene = aimp.ReadFile(
            ip, aiProcessPreset_TargetRealtime_Fast
        );  // aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_GenBoundingBoxes | aiProcess_FlipUVs);
        std::cout << "\t\t " << scene->mNumMeshes << " meshes, " << scene->mNumMaterials << " materials\n";

        size_t start_mat_index = out.num_materials();
        for(size_t i = 0; i < scene->mNumMeshes; ++i)
            load_mesh(scene->mMeshes[i], scene, start_mat_index);

        for(size_t i = 0; i < scene->mNumMaterials; ++i) {
            const auto*   mat = scene->mMaterials[i];
            material_info info;
            for(auto tt : {aiTextureType_DIFFUSE, aiTextureType_NORMALS, aiTextureType_METALNESS, aiTextureType_SHININESS}) {
                aiString tpath;
                if(mat->GetTexture(tt, 0, &tpath) == aiReturn_SUCCESS) {
                    std::string tp{tpath.C_Str()};
                    for(char& i : tp)
                        if(i == '\\') i = '/';
                    info.set_texture(tt, add_texture_path(ip.parent_path() / tp));
                }
            }
            out.add_material(std::move(info));
        }
    }

    void load_texture(texture_id id, const path& ip) {
        std::cout << "\t" << ip << " (" << id << ") \n";
        int   x, y, channels;
        auto* data = stbi_load(ip.c_str(), &x, &y, &channels, STBI_default);
        if(data == nullptr) {
            std::cout << "\t\tfailed to load texture " << ip << ": " << stbi_failure_reason() << "\n";
            return;
        }
        std::cout << "\t\tloaded texture " << x << "x" << y << " c=" << channels << "\n";
        out.add_texture(id, ip.filename(), x, y, channels, data);
    }

    void load() {
        std::cout << "loading models:\n";
        for(const auto& ip : models)
            load_model(ip);

        std::cout << "loading textures:\n";
        for(const auto& [id, ip] : textures)
            load_texture(id, ip);
    }
};

int main(int argc, char* argv[]) {
    if(argc < 2) {
        std::cout << "usage:\n\tasset-bundler <output bundle path> <input asset path>...\n";
        return -1;
    }

    const std::unordered_set<std::string> texture_exts = {".png", ".jpg", ".bmp"};

    output_bundle out{argv[1]};
    importer      imp{out};
    for(size_t i = 2; i < argc; ++i) {
        path input = argv[i];
        if(imp.aimp.IsExtensionSupported(input.extension()))
            imp.models.emplace_back(input);
        else if(texture_exts.find(input.extension()) != texture_exts.end())
            imp.add_texture_path(input);
        else
            std::cout << "unknown file type: " << input.extension() << "\n";
    }

    imp.load();

    out.write();

    return 0;
}
