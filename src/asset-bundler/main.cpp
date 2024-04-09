#include "asset-bundler/importer.h"
#include "asset-bundler/model.h"
#include "asset-bundler/output_bundle.h"
#include "asset-bundler/texture_processor.h"

/* asset-bundler:
 *  content pipeline & bundling utility
 *  roles:
 *      - load assets (3D models [meshes, materials, textures by reference], textures, environment
 * maps)
 *      - transform assets into a format suitable for rendering pipeline
 *      - represent them in a uniform way
 *      - bundle them so they can be loaded quickly
 *  usage:
 *      asset-bundler <output bundle name> <input assets>...
 */
int main(int argc, char* argv[]) {
    if(argc < 2) {
        std::cout << "usage:\n\tasset-bundler <output bundle path> <input asset path>...\n";
        return -1;
    }

    std::filesystem::path              output_path;
    std::vector<std::filesystem::path> input_paths;
    options                            opts;

    for(int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if(arg == "--no-ibl-precomp")
            opts.enable_ibl_precomputation = false;
        else if(output_path.empty())
            output_path = arg;
        else
            input_paths.emplace_back(arg);
    }

    texture_processor tex_proc{opts};
    output_bundle     out{output_path, &tex_proc};
    importer          imp{out, input_paths};
    imp.load();
    out.write();
    return 0;
}
