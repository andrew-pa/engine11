
add_library(egg-forward-renderer SHARED
    shared.cpp
    ../../memory.cpp
    forward.cpp)
target_compile_features(egg-forward-renderer PUBLIC cxx_std_20)
add_shaders(egg-forward-renderer
    ENV vulkan1.3
    FORMAT num
    SOURCES
        forward.vert
        forward.frag)
target_link_libraries(egg-forward-renderer
    glm
    Vulkan::Vulkan VulkanMemoryAllocator)
