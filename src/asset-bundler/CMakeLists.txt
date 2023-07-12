
# TODO: make egg/memory.cpp global
add_executable(asset-bundler
    main.cpp output_bundle.cpp importer.cpp texture_processor.cpp
    ${PROJECT_SOURCE_DIR}/src/egg/renderer/memory.cpp)
target_compile_features(asset-bundler PUBLIC cxx_std_20)
target_link_libraries(asset-bundler glm libzstd_static assimp stblib
    Vulkan::Vulkan VulkanMemoryAllocator vmalib)
