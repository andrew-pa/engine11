
add_executable(egg
    main.cpp
    renderer/renderer.cpp
    renderer/core/frame_renderer.cpp
    renderer/core/device.cpp
    renderer/imgui_renderer.cpp
    renderer/scene_renderer.cpp
    renderer/gpu_static_scene_data.cpp
    renderer/memory.cpp
    input/input_distributor.cpp
    input/camera_interaction_models.cpp
    shared_library_reloader.cpp
    components.cpp
    bundle.cpp)
target_compile_features(egg PUBLIC cxx_std_20)
target_compile_definitions(egg PRIVATE
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VULKAN_HPP_STORAGE_SHARED
    VULKAN_HPP_STORAGE_SHARED_EXPORT)
target_link_libraries(egg
    flecs::flecs_static
    glm
    glfw
    Vulkan::Vulkan VulkanMemoryAllocator vmalib
    imguilib
    libzstd_shared)
# make sure that the rendering algorithm shared library can access the dynamic Vulkan-Hpp dispatcher symbol.
set_target_properties(egg PROPERTIES ENABLE_EXPORTS ON)
