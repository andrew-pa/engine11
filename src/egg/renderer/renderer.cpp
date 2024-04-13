#include "egg/renderer/renderer.h"
#include "asset-bundler/format.h"
#include "egg/renderer/core/frame_renderer.h"
#include "egg/renderer/imgui_renderer.h"
#include "egg/renderer/memory.h"
#include "egg/renderer/scene_renderer.h"
#include "egg/shared_library_reloader.h"
#include "error.h"
#include <fs-shim.h>
#include <iostream>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_structs.hpp>

using create_rendering_algorithm_f = rendering_algorithm* (*)();

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
    VkDebugUtilsMessageTypeFlagsEXT             message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void*                                       user_data
) {
    // if(std::strncmp("Device Extension", msg, 16) == 0) return VK_FALSE;

    switch(message_severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: std::cout << "[DEBUG]"; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            std::cout << "[\033[32mINFO\033[0m]";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            std::cout << "[\033[33mWARN\033[0m]";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
            std::cout << "\n[\033[31mERROR\033[0m]";
            break;
    }

    std::cout << " " << callback_data->pMessage << "\n";

    return VK_FALSE;
}

const vk::ApplicationInfo APP_INFO = vk::ApplicationInfo{
    "egg", VK_MAKE_VERSION(0, 0, 0), "egg", VK_MAKE_VERSION(0, 0, 0), VK_API_VERSION_1_3
};

vk::Extent2D get_window_extent(GLFWwindow* window) {
    vk::Extent2D e;
    glfwGetWindowSize(window, (int*)&e.width, (int*)&e.height);
    return e;
}

renderer::renderer(
    GLFWwindow*                   window,
    std::shared_ptr<flecs::world> world,
    const std::filesystem::path&  rendering_algorithm_library_path
)
    : rendering_algo_lib_loader(new shared_library_reloader(rendering_algorithm_library_path)) {
    // create Vulkan instance
    uint32_t                 glfw_ext_count = 0;
    auto*                    glfw_req_exts  = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
    std::vector<const char*> extensions{glfw_req_exts, glfw_req_exts + glfw_ext_count};
    // extensions.push_back("VK_KHR_portability_enumeration");
#ifndef NDEBUG
    extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    instance = vk::createInstanceUnique(vk::InstanceCreateInfo{
        vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR,
        &APP_INFO,
        0,
        {},
        (uint32_t)extensions.size(),
        extensions.data()
    });

    // set up vulkan debugging reports
#ifndef NDEBUG
    auto cbco = vk::DebugUtilsMessengerCreateInfoEXT{
        {},
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo
            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose,
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
            | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
            | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        debug_callback
    };
    auto err1 = instance->createDebugUtilsMessengerEXT(
        &cbco,
        nullptr,
        &debug_report_callback,
        vk::DispatchLoaderDynamic(instance.get(), vkGetInstanceProcAddr)
    );
    if(err1 != vk::Result::eSuccess) {
        std::cout << "WARNING: failed to create debug report callback: " << vk::to_string(err1)
                  << "\n";
    }
#endif
    // create window surface
    VkSurfaceKHR surface;

    auto err = glfwCreateWindowSurface(instance.get(), window, nullptr, &surface);
    if(err != VK_SUCCESS) {
        for(const auto* ext : extensions)
            std::cout << "requested extension: " << ext << "\n";
        throw vulkan_runtime_error("failed to create window surface", err);
    }

    window_surface = vk::UniqueSurfaceKHR(surface, {instance.get()});

    auto* init_lib = rendering_algo_lib_loader->initial_load();
    auto  crafn = (create_rendering_algorithm_f)load_symbol(init_lib, "create_rendering_algorithm");

    auto* algo = crafn();

    init_device(instance.get(), algo->required_features());

    command_pool = dev->createCommandPoolUnique(vk::CommandPoolCreateInfo{
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer, graphics_queue_family_index
    });

    // read surface capabilities, which should be stable throughout execution
    auto surface_caps   = phy_dev.getSurfaceCapabilitiesKHR(window_surface.get());
    surface_image_count = std::max(surface_caps.minImageCount, 3u);
    std::cout << "using a swap chain with " << surface_image_count << " images\n";

    auto fmts = phy_dev.getSurfaceFormatsKHR(window_surface.get());
    for(auto fmt : fmts)
        std::cout << "available format " << vk::to_string(fmt.format) << " / "
                  << vk::to_string(fmt.colorSpace) << "\n";
    surface_format = fmts[0];

    std::cout << "using format " << vk::to_string(surface_format.format) << " / "
              << vk::to_string(surface_format.colorSpace) << "\n";

    // create different rendering layers
    fr = new frame_renderer(this, get_window_extent(window));
    ir = new imgui_renderer(this, window);
    ir->create_swapchain_depd(fr);
    sr = new scene_renderer(this, std::move(world), algo);
    sr->create_swapchain_depd(fr);
}

renderer::~renderer() {
    graphics_queue.waitIdle();
    present_queue.waitIdle();

    delete sr;
    delete rendering_algo_lib_loader;

    delete ir;
    delete fr;

    command_pool.reset();
    upload_cmds.reset();
    upload_fence.reset();

    allocator.reset();
    window_surface.reset();
    dev.reset();
    instance->destroyDebugUtilsMessengerEXT(
        debug_report_callback,
        nullptr,
        vk::DispatchLoaderDynamic(instance.get(), vkGetInstanceProcAddr)
    );
    instance.reset();
}

void renderer::start_resource_upload(const std::shared_ptr<asset_bundle>& assets) {
    upload_cmds  = std::move(dev->allocateCommandBuffersUnique(
        vk::CommandBufferAllocateInfo{command_pool.get(), vk::CommandBufferLevel::ePrimary, 1}
    )[0]);
    upload_fence = dev->createFenceUnique(vk::FenceCreateInfo{});

    upload_cmds->begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    sr->start_resource_upload(assets, upload_cmds.get());
    ir->start_resource_upload(upload_cmds.get());

    upload_cmds->end();
    graphics_queue.submit(
        vk::SubmitInfo{0, nullptr, nullptr, 1, &upload_cmds.get()}, upload_fence.get()
    );
}

void renderer::wait_for_resource_upload_to_finish() {
    // do setup that doesn't depend on the command buffer while we wait for the upload to finish
    sr->setup_scene_post_upload();

    auto err = dev->waitForFences(upload_fence.get(), VK_TRUE, UINT64_MAX);
    if(err != vk::Result::eSuccess)
        throw vulkan_runtime_error("failed to wait for resource upload", err);

    sr->resource_upload_cleanup();
    ir->resource_upload_cleanup();
    upload_cmds.reset();
}

void renderer::resize(GLFWwindow* window) {
    fr->reset_swapchain(get_window_extent(window));
    ir->create_swapchain_depd(fr);
    sr->create_swapchain_depd(fr);
}

void renderer::render_frame() {
    rendering_algo_lib_loader->poll([&](void* new_lib) {
        auto crafn
            = (create_rendering_algorithm_f)load_symbol(new_lib, "create_rendering_algorithm");
        auto* old_algo = sr->swap_rendering_algorithm(crafn());
        graphics_queue.waitIdle();
        present_queue.waitIdle();
        delete old_algo;
    });

    auto frame = fr->begin_frame();
    sr->render_frame(frame);
    ir->render_frame(frame);
    fr->end_frame(std::move(frame));
}
