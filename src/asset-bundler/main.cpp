#include "asset-bundler/importer.h"
#include "asset-bundler/model.h"
#include "asset-bundler/output_bundle.h"
#include "asset-bundler/texture_processor.h"

/* asset-bundler:
 *  content pipeline & bundling utility
 *  roles:
 *      - load assets (3D models [meshes, materials, textures by reference], textures)
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

    texture_processor tex_proc;
    output_bundle out{argv[1], &tex_proc};
    importer      imp{out, argc, argv, &tex_proc};
    imp.load();
    out.write();
    return 0;
}
