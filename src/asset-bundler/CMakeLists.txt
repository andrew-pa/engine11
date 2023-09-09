
# TODO: make egg/memory.cpp global
add_executable(asset-bundler
    main.cpp output_bundle.cpp importer.cpp texture_processor.cpp
    base_process_job.cpp envmap_process_job.cpp texture_process_job.cpp
    ${PROJECT_SOURCE_DIR}/src/egg/renderer/memory.cpp)
target_compile_features(asset-bundler PUBLIC cxx_std_20)
add_shaders(asset-bundler
    ENV vulkan1.3
    FORMAT num
    SOURCES
        skybox.comp)
target_link_libraries(asset-bundler glm libzstd_static assimp stblib
    Vulkan::Vulkan VulkanMemoryAllocator vmalib)
