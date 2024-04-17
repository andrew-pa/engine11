#include "asset-bundler/output_bundle.h"
#include "asset-bundler/format.h"
#include "asset-bundler/texture_processor.h"
#include "fs-shim.h"
#include <vulkan/vulkan_format_traits.hpp>
#include <zstd.h>

void output_bundle::add_texture(
    texture_id         id,
    const std::string& name,
    uint32_t           width,
    uint32_t           height,
    int                nchannels,
    stbi_uc*           data
) {
    string_id    ns = add_string(name);
    texture_info info{ns, width, height, format_from_channels(nchannels), data};
    tex_proc->submit_texture(id, &info);
    textures.emplace(id, info);
}

void output_bundle::add_environment(
    const std::string& name, uint32_t width, uint32_t height, int nchannels, float* data
) {
    string_id ns   = add_string(name);
    auto      info = tex_proc->submit_environment(ns, width, height, nchannels, data);
    environments.emplace_back(info);
}

const size_t MAX_TEXTURE_ALIGN = 16;

std::pair<size_t, size_t> output_bundle::estimate_sizes() const {
    size_t total = sizeof(asset_bundle_format::header);
    total += sizeof(asset_bundle_format::string_header) * strings.size();
    total += sizeof(asset_bundle_format::texture_header) * textures.size();
    total += sizeof(asset_bundle_format::material_header) * materials.size();
    total += sizeof(asset_bundle_format::mesh_header) * meshes.size();
    total += sizeof(asset_bundle_format::object_header) * objects.size();
    total += sizeof(asset_bundle_format::group_header) * groups.size();
    total += sizeof(asset_bundle_format::environment_header) * environments.size();
    size_t header_size = total;

    for(const auto& s : strings)
        total += s.second.size();
    for(const auto& t : textures)
        total += t.second.len + MAX_TEXTURE_ALIGN;
    for(const auto& o : objects)
        total += o.mesh_indices.size() * sizeof(uint32_t);
    for(const auto& o : groups)
        total += o.objects.size() * sizeof(object_id);
    for(const auto& e : environments)
        total += e.len + MAX_TEXTURE_ALIGN;
    total += sizeof(vertex) * vertices.size();
    total += sizeof(index_type) * indices.size();
    return {header_size, total};
}

void output_bundle::write() {
    // compute total uncompressed size & allocate buffer (RIP this might use a lot of RAM)
    auto [header_size, max_size] = estimate_sizes();
    std::cout << "bundle max size " << max_size << " bytes\n";
    byte* buffer = (byte*)malloc(max_size);
    assert(buffer != nullptr);

    // copy data into buffer in correct format
    std::cout << "copying data...\n";
    auto* header = (asset_bundle_format::header*)buffer;
    *header
        = {.num_strings        = strings.size(),
           .num_textures       = textures.size(),
           .num_materials      = materials.size(),
           .num_meshes         = meshes.size(),
           .num_objects        = objects.size(),
           .num_groups         = groups.size(),
           .num_environments   = environments.size(),
           .num_total_vertices = vertices.size(),
           .num_total_indices  = indices.size(),
           .data_offset        = header_size};
    std::cout << "creating a bundle with\n"
              << "\t# strings = " << header->num_strings << "\n"
              << "\t# textures = " << header->num_textures << "\n"
              << "\t# materials = " << header->num_materials << "\n"
              << "\t# meshes = " << header->num_meshes << "\n"
              << "\t# objects = " << header->num_objects << "\n"
              << "\t# groups = " << header->num_groups << "\n"
              << "\t# environments = " << header->num_environments << "\n";

    byte* header_ptr = buffer + sizeof(asset_bundle_format::header);
    byte* data_ptr   = buffer + header_size;
    // CPU only data
    copy_strings(header_ptr, data_ptr, buffer);
    copy_materials(header_ptr);
    copy_meshes(header_ptr);
    copy_objects(header_ptr, data_ptr, buffer);
    copy_groups(header_ptr, data_ptr, buffer);

    // everything that needs to go on the GPU (CPU headers will also be in the same order)
    // TODO: we could easily eliminate this field by just passing `buffer + header_size` as `top`
    // and including it in the offset for each resource
    header->gpu_data_offset = (size_t)(data_ptr - buffer);
    copy_textures(header_ptr, data_ptr, buffer);

    if(!environments.empty()) copy_environments(header_ptr, data_ptr, buffer);

    header->vertex_start_offset = (size_t)(data_ptr - buffer);
    memcpy(data_ptr, vertices.data(), vertices.size() * sizeof(vertex));
    data_ptr += vertices.size() * sizeof(vertex);

    header->index_start_offset = (size_t)(data_ptr - buffer);
    memcpy(data_ptr, indices.data(), indices.size() * sizeof(index_type));
    data_ptr += indices.size() * sizeof(index_type);

    auto actual_size = data_ptr - buffer;
    std::cout << "actual size: " << actual_size << " <= " << max_size << " d"
              << (actual_size - max_size) << "\n";
    assert(actual_size <= max_size);

    // compress data and write it to file
    std::cout << "compressing bundle...\n";
    size_t compressed_buffer_size = ZSTD_compressBound(actual_size);
    byte*  compressed_buffer      = (byte*)malloc(compressed_buffer_size);
    // TODO: make compression level configurable
    size_t actual_compressed_size = ZSTD_compress(
        compressed_buffer, compressed_buffer_size, buffer, actual_size, ZSTD_minCLevel() + 2
    );
    free(buffer);
    auto percent_compressed = ((double)(actual_compressed_size) / (double)(actual_size)) * 100.0;
    std::cout << "writing output (" << actual_compressed_size << " bytes, " << percent_compressed
              << "%)...\n";
    auto* f             = fopen(path_to_string(output_path).c_str(), "wb");
    auto  bytes_written = fwrite(compressed_buffer, 1, actual_compressed_size, f);
    std::cout << "wrote " << bytes_written << " bytes " << ferror(f) << "\n";
    assert(bytes_written == actual_compressed_size);
    free(compressed_buffer);
    fclose(f);
    std::cout << "finished!\n";
}

void output_bundle::copy_strings(byte*& header_ptr, byte*& data_ptr, byte* top) const {
    for(const auto& s : strings) {
        *((asset_bundle_format::string_header*)header_ptr) = asset_bundle_format::string_header{
            .id = s.first, .offset = (size_t)(data_ptr - top), .len = s.second.size()
        };
        header_ptr += sizeof(asset_bundle_format::string_header);
        memcpy(data_ptr, s.second.data(), s.second.size());
        data_ptr += s.second.size();
    }
}

inline void move_to_next_multiple(byte*& p, uint8_t m) { p = p + (m - ((uintptr_t)p % m)) % m; }

void output_bundle::copy_textures(byte*& header_ptr, byte*& data_ptr, byte* top) const {
    for(const auto& t : textures) {
        move_to_next_multiple(data_ptr, vk::blockSize(t.second.img.format));
        *((asset_bundle_format::texture_header*)header_ptr) = asset_bundle_format::texture_header{
            .id     = t.first,
            .name   = t.second.name,
            .img    = t.second.img.as_image(),
            .offset = (size_t)(data_ptr - top)
        };
        header_ptr += sizeof(asset_bundle_format::texture_header);
        // check to see if this texture was processed on the GPU
        if(t.second.data == nullptr)
            tex_proc->recieve_processed_texture(t.first, data_ptr);
        else
            memcpy(data_ptr, t.second.data, t.second.len);
        data_ptr += t.second.len;
    }
}

void output_bundle::copy_environments(byte*& header_ptr, byte*& data_ptr, byte* top) const {
    for(const auto& e : environments) {
        move_to_next_multiple(data_ptr, vk::blockSize(e.skybox.format));
        *((asset_bundle_format::environment_header*)header_ptr)
            = asset_bundle_format::environment_header{
                .name                      = e.name,
                .skybox                    = e.skybox.as_image(),
                .skybox_offset             = (size_t)(data_ptr - top),
                .diffuse_irradiance        = e.diffuse_irradiance.as_image(),
                .diffuse_irradiance_offset = (size_t)(data_ptr - top) + e.diffuse_irradiance_offset,
            };
        header_ptr += sizeof(asset_bundle_format::environment_header);
        tex_proc->recieve_processed_environment(e.name, data_ptr);
        data_ptr += e.len;
    }
}

void output_bundle::copy_materials(byte*& header_ptr) const {
    size_t s = materials.size() * sizeof(asset_bundle_format::material_header);
    // !!! Assumes that material_header === material_info
    memcpy(header_ptr, materials.data(), s);
    header_ptr += s;
}

void output_bundle::copy_meshes(byte*& header_ptr) const {
    size_t s = meshes.size() * sizeof(asset_bundle_format::mesh_header);
    // !!! Assumes that mesh_header === mesh_info
    memcpy(header_ptr, meshes.data(), s);
    header_ptr += s;
}

void output_bundle::copy_objects(byte*& header_ptr, byte*& data_ptr, byte* top) const {
    for(const auto& o : objects) {
        auto* h = ((asset_bundle_format::object_header*)header_ptr);
        *h      = asset_bundle_format::object_header{
                 .name             = o.name,
                 .num_meshes       = (uint32_t)o.mesh_indices.size(),
                 .offset           = (size_t)(data_ptr - top),
                 .transform_matrix = o.transform,
                 .bounds           = o.bounds
        };
        header_ptr += sizeof(asset_bundle_format::object_header);
        memcpy(data_ptr, o.mesh_indices.data(), o.mesh_indices.size() * sizeof(uint32_t));
        data_ptr += o.mesh_indices.size() * sizeof(uint32_t);
    }
}

void output_bundle::copy_groups(byte*& header_ptr, byte*& data_ptr, byte* top) const {
    for(const auto& o : groups) {
        auto* h = ((asset_bundle_format::group_header*)header_ptr);
        *h      = asset_bundle_format::group_header{
                 .name        = o.name,
                 .num_objects = (uint32_t)o.objects.size(),
                 .offset      = (size_t)(data_ptr - top),
                 .bounds      = o.bounds
        };
        header_ptr += sizeof(asset_bundle_format::group_header);
        memcpy(data_ptr, o.objects.data(), o.objects.size() * sizeof(object_id));
        data_ptr += o.objects.size() * sizeof(object_id);
    }
}

output_bundle::~output_bundle() {
    for(const auto& [id, ifo] : textures)
        free(ifo.data);
}
