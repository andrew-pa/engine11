#include "asset-bundler/output_bundle.h"

void output_bundle::add_texture(
    texture_id id, std::string name, uint32_t width, uint32_t height, int nchannels, stbi_uc* data
) {
    string_id ns = add_string(std::move(name));
    // check if the texture is silly and we can store it as a single texel
    // the output stage will truncate the data when it copies it into the file
    if(texture_is_single_value(width, height, nchannels, data)) {
        width  = 1;
        height = 1;
    }
    // TODO: we should probably also compute mipmaps
    textures.emplace(id, texture_info{ns, width, height, format_from_channels(nchannels), data, width * height * nchannels});
}

void output_bundle::write() {}

output_bundle::~output_bundle() {
    for(const auto& [id, ifo] : textures)
        stbi_image_free(ifo.data);
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
