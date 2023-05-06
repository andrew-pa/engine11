
add_executable(asset-bundler
    main.cpp output_bundle.cpp importer.cpp)
target_compile_features(asset-bundler PUBLIC cxx_std_20)
target_link_libraries(asset-bundler glm libzstd_static assimp stblib Vulkan::Vulkan)
