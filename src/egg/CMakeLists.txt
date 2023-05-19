
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
target_link_libraries(egg
    flecs::flecs_static
    glm
    glfw
    Vulkan::Vulkan VulkanMemoryAllocator
    imguilib
    libzstd_static)
