#include "asset-bundler/importer.h"

const std::unordered_set<std::string> texture_exts = {".png", ".jpg", ".bmp"};

importer::importer(output_bundle& out, int argc, char* argv[]) : out(out) {
    for(size_t i = 2; i < argc; ++i) {
        path input = argv[i];
        if(aimp.IsExtensionSupported(input.extension()))
            models.emplace_back(input);
        else if(texture_exts.find(input.extension()) != texture_exts.end())
            add_texture_path(input);
        else
            std::cout << "unknown file type: " << input.extension() << "\n";
    }
}

void importer::load_mesh(const aiMesh* m, const aiScene* scene, size_t mat_index_offset) {
    std::cout << "\t\t " << m->mName.C_Str() << " ("
              << scene->mMaterials[m->mMaterialIndex]->GetName().C_Str() << ") " << m->mNumVertices
              << " vertices, " << m->mNumFaces << " faces"
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

// only support two levels: groups and objects? makes scene graphs simpler but ECS probably won't be
// a scene graph itself????
void importer::load_graph(const aiNode* node) {
    for(size_t i = 0; i < node->mNumChildren; ++i) {
        auto* c = node->mChildren[i];
        if(c->mNumMeshes > 0 && c->mNumChildren == 0)
            load_object(c);
        else
            load_group(c);
    }
}

void importer::load_group(const aiNode* node) {
    std::cout << "\t\t\t group: " << node->mName.C_Str() << "\n";
    std::vector<object_id> members;
    members.reserve(node->mNumChildren);
    for(size_t i = 0; i < node->mNumChildren; ++i) {
        auto* c = node->mChildren[i];
        if(c->mNumMeshes > 0 && c->mNumChildren == 0)
            members.emplace_back(load_object(c));
        else
            std::cout << "\t\t\t\t invalid subgroup " << c->mNumChildren << "\n";
    }
    out.add_group({out.add_string(node->mName.C_Str()), members});
}

object_id importer::load_object(const aiNode* node) {
    std::cout << "\t\t\t\t object: " << node->mName.C_Str() << " " << node->mNumMeshes
              << " meshes \n";
    // if(!node->mTransformation.IsIdentity()) std::cout << "\t\t\t\t\t transform != identity\n";
    std::vector<uint32_t> meshes;
    meshes.reserve(node->mNumMeshes);
    for(size_t i = 0; i < node->mNumMeshes; ++i)
        // !!! Assume that we load meshes after we load the graph
        meshes.emplace_back(node->mMeshes[i] + out.num_meshes());
    return out.add_object({out.add_string(node->mName.C_Str()), meshes, node->mTransformation});
}

void importer::load_model(const path& ip) {
    std::cout << "\t" << ip << "\n";
    // TODO: why does aiProcessPreset_TargetRealtime_MaxQuality seg fault because it doesn't
    // generate tangents??
    const aiScene* scene
        = aimp.ReadFile(ip, aiProcessPreset_TargetRealtime_Fast | aiProcess_FlipUVs);
    std::cout << "\t\t" << scene->mNumMeshes << " meshes, " << scene->mNumMaterials
              << " materials\n";

    load_graph(scene->mRootNode);

    size_t start_mat_index = out.num_materials();
    for(size_t i = 0; i < scene->mNumMeshes; ++i)
        load_mesh(scene->mMeshes[i], scene, start_mat_index);

    for(size_t i = 0; i < scene->mNumMaterials; ++i) {
        const auto*   mat = scene->mMaterials[i];
        material_info info{out.add_string(mat->GetName().C_Str())};
        for(auto tt :
            {aiTextureType_DIFFUSE,
             aiTextureType_NORMALS,
             aiTextureType_METALNESS,
             aiTextureType_SHININESS}) {
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

void importer::load_texture(texture_id id, const path& ip) {
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

void importer::load() {
    std::cout << "loading models:\n";
    for(const auto& ip : models)
        load_model(ip);

    std::cout << "loading textures:\n";
    for(const auto& [id, ip] : textures)
        load_texture(id, ip);
}
