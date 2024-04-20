#pragma once
#include <filesystem>
#define GLFW_INCLUDE_VULKAN
#include "egg/bundle.h"
#include "egg/renderer/memory.h"
#include "egg/renderer/renderer_shared.h"
#include "mem_arena.h"
#include <GLFW/glfw3.h>
#include <flecs.h>
#include <memory>

/*
 * responsibilities:
    initializing Vulkan
    low level rendering: swap chain, render passes
    imgui init & render
    high level rendering: upload scene onto GPU
    render pipeline
*/
class renderer {
    vk::UniqueInstance         instance;
    vk::DebugUtilsMessengerEXT debug_report_callback;

    vk::UniqueSurfaceKHR window_surface;
    vk::SurfaceFormatKHR surface_format;
    uint32_t             surface_image_count;

    vk::UniqueDevice               dev;
    vk::PhysicalDevice             phy_dev;
    uint32_t                       graphics_queue_family_index, present_queue_family_index;
    vk::Queue                      graphics_queue, present_queue;
    std::shared_ptr<gpu_allocator> allocator;
    void init_device(vk::Instance instance, const renderer_features& required_features);

    vk::UniqueCommandPool command_pool;

    vk::UniqueCommandBuffer upload_cmds;
    vk::UniqueFence         upload_fence;

    frame_renderer* fr;
    imgui_renderer* ir;
    scene_renderer* sr;

    class shared_library_reloader* rendering_algo_lib_loader;

    arena<uint8_t>                extra_properties;
    vk::PhysicalDeviceProperties2 dev_props;

  public:
    renderer(
        GLFWwindow*                   window,
        std::shared_ptr<flecs::world> world,
        const std::filesystem::path&  rendering_algorithm_library_path
    );

    void start_resource_upload(const std::shared_ptr<asset_bundle>& assets);
    void wait_for_resource_upload_to_finish();

    void resize(GLFWwindow* window);

    void render_frame();

    inline vk::Instance vulkan_instance() const { return instance.get(); }

    inline vk::Device device() const { return dev.get(); }

    inline std::shared_ptr<gpu_allocator> gpu_alloc() const { return allocator; }

    inline abstract_imgui_renderer* imgui() const { return (abstract_imgui_renderer*)ir; }

    const vk::PhysicalDeviceProperties2& get_physical_device_properties() const {
        return dev_props;
    }

    template<typename T>
    const T& get_physical_device_property() const {
        const auto stype = T::structureType;

        struct unknown {
            vk::StructureType sType;
            unknown*          next;
        };

        auto* cur = (unknown*)dev_props.pNext;
        while(cur != nullptr) {
            if(cur->sType == stype) return *(T*)cur;
            cur = cur->next;
        }
        throw std::runtime_error(
            std::string("physical device properties did not contain structure: ")
            + vk::to_string(stype)
        );
    }

    renderer(renderer&)            = delete;
    renderer& operator=(renderer&) = delete;

    ~renderer();

    friend class frame_renderer;
    friend class imgui_renderer;
    friend class scene_renderer;
};
