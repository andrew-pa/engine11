#include "asset-bundler/output_bundle.h"
#include "asset-bundler/format.h"
#include "fs-shim.h"
#include <zstd.h>

void output_bundle::add_texture(
    texture_id id, std::string name, uint32_t width, uint32_t height, int nchannels, stbi_uc* data
) {
    string_id ns = add_string(std::move(name));
    textures.emplace(
        id,
        texture_info{
            ns, width, height, format_from_channels(nchannels), data, width * height * nchannels}
    );
}

std::pair<size_t, size_t> output_bundle::total_and_header_size() const {
    size_t total = sizeof(asset_bundle_format::header);
    total += sizeof(asset_bundle_format::string_header) * strings.size();
    total += sizeof(asset_bundle_format::texture_header) * textures.size();
    total += sizeof(asset_bundle_format::material_header) * materials.size();
    total += sizeof(asset_bundle_format::mesh_header) * meshes.size();
    total += sizeof(asset_bundle_format::object_header) * objects.size();
    total += sizeof(asset_bundle_format::group_header) * groups.size();
    size_t header_size = total;

    for(const auto& s : strings)
        total += s.second.size();
    for(const auto& t : textures)
        total += t.second.len;
    for(const auto& o : objects)
        total += o.mesh_indices.size() * sizeof(uint32_t);
    for(const auto& o : groups)
        total += o.objects.size() * sizeof(object_id);
    total += sizeof(vertex) * vertices.size();
    total += sizeof(index_type) * indices.size();
    return {header_size, total};
}

void output_bundle::write() {
    // compute total uncompressed size & allocate buffer (RIP this might use a lot of RAM)
    auto [header_size, total_size] = total_and_header_size();
    std::cout << "bundle total size " << total_size << " bytes\n";
    byte* buffer = (byte*)malloc(total_size);
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
           .num_total_vertices = vertices.size(),
           .num_total_indices  = indices.size(),
           .data_offset        = header_size};
    byte* header_ptr = buffer + sizeof(asset_bundle_format::header);
    byte* data_ptr   = buffer + header_size;
    copy_strings(header_ptr, data_ptr, buffer);
    copy_materials(header_ptr);
    copy_meshes(header_ptr);
    copy_objects(header_ptr, data_ptr, buffer);
    copy_groups(header_ptr, data_ptr, buffer);

    header->gpu_data_offset = (size_t)(data_ptr - buffer);
    copy_textures(header_ptr, data_ptr, buffer);
    header->vertex_start_offset = (size_t)(data_ptr - buffer);
    memcpy(data_ptr, vertices.data(), vertices.size() * sizeof(vertex));
    data_ptr += vertices.size() * sizeof(vertex);
    header->index_start_offset = (size_t)(data_ptr - buffer);
    memcpy(data_ptr, indices.data(), indices.size() * sizeof(index_type));
    data_ptr += indices.size() * sizeof(index_type);
    assert((data_ptr - buffer) == total_size);

    // compress data and write it to file
    std::cout << "compressing bundle...\n";
    size_t compressed_buffer_size = ZSTD_compressBound(total_size);
    byte*  compressed_buffer      = (byte*)malloc(compressed_buffer_size);
    size_t actual_compressed_size = ZSTD_compress(
        compressed_buffer, compressed_buffer_size, buffer, total_size, ZSTD_defaultCLevel()
    );
    free(buffer);
    auto* f = fopen(path_to_string(output_path).c_str(), "wb");
    std::cout << "writing output (" << actual_compressed_size << " bytes)...\n";
    auto bytes_written = fwrite(compressed_buffer, 1, actual_compressed_size, f);
    std::cout << "wrote " << bytes_written << " bytes " << ferror(f) << "\n";
    assert(bytes_written == actual_compressed_size);
    free(compressed_buffer);
    fclose(f);
    std::cout << "finished!\n";
}

void output_bundle::copy_strings(byte*& header_ptr, byte*& data_ptr, byte* top) const {
    for(const auto& s : strings) {
        *((asset_bundle_format::string_header*)header_ptr) = asset_bundle_format::string_header{
            .id = s.first, .offset = (size_t)(data_ptr - top), .len = s.second.size()};
        header_ptr += sizeof(asset_bundle_format::string_header);
        memcpy(data_ptr, s.second.data(), s.second.size());
        data_ptr += s.second.size();
    }
}

void output_bundle::copy_textures(byte*& header_ptr, byte*& data_ptr, byte* top) const {
    for(const auto& t : textures) {
        *((asset_bundle_format::texture_header*)header_ptr) = asset_bundle_format::texture_header{
            .id     = t.first,
            .name   = t.second.name,
            .width  = t.second.width,
            .height = t.second.height,
            .format = (VkFormat)t.second.format,
            .offset = (size_t)(data_ptr - top)};
        header_ptr += sizeof(asset_bundle_format::texture_header);
        memcpy(data_ptr, t.second.data, t.second.len);
        data_ptr += t.second.len;
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
                 .name       = o.name,
                 .num_meshes = (uint32_t)o.mesh_indices.size(),
                 .offset     = (size_t)(data_ptr - top),
        };
        // !!! Assumes these have the same shape in memory
        memcpy(&h->transform_matrix, &o.transform, sizeof(float) * 16);
        h->transform_matrix = glm::transpose(h->transform_matrix);
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
        };
        // !!! Assumes these have the same shape in memory
        header_ptr += sizeof(asset_bundle_format::group_header);
        memcpy(data_ptr, o.objects.data(), o.objects.size() * sizeof(object_id));
        data_ptr += o.objects.size() * sizeof(object_id);
    }
}

output_bundle::~output_bundle() {
    for(const auto& [id, ifo] : textures)
        free(ifo.data);
}
