#include "asset-bundler/importer.h"
#include "asset-bundler/model.h"
#include "asset-bundler/output_bundle.h"

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

    output_bundle out{argv[1]};
    importer      imp{out, argc, argv};
    imp.load();
    out.write();
    return 0;
}
