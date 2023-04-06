#include "egg/renderer/renderer.h"
#include "egg/renderer/core/frame_renderer.h"
#include "egg/renderer/imgui_renderer.h"
#include "egg/renderer/scene_renderer.h"
#include <iostream>

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugReportFlagsEXT      flags,
    VkDebugReportObjectTypeEXT objType,
    uint64_t                   obj,
    size_t                     location,
    int32_t                    code,
    const char*                layerPrefix,
    const char*                msg,
    void*                      userData
) {
    if(std::strncmp("Device Extension", msg, 16) == 0) return VK_FALSE;
    vk::DebugReportFlagsEXT flg{(vk::DebugReportFlagBitsEXT)flags};
    if(flg & vk::DebugReportFlagBitsEXT::eDebug)
        std::cout << "[DEBUG]";
    else if(flg & vk::DebugReportFlagBitsEXT::eError)
        std::cout << "\n[\033[31mERROR\033[0m]";
    else if(flg & vk::DebugReportFlagBitsEXT::eInformation)
        std::cout << "[\033[32mINFO\033[0m]";
    else if(flg & vk::DebugReportFlagBitsEXT::ePerformanceWarning)
        std::cout << "[PERF]";
    else if(flg & vk::DebugReportFlagBitsEXT::eWarning)
        std::cout << "[\033[33mWARN\033[0m]";
    std::cout << ' ' /*<< layerPrefix << " | "*/ << msg << "\n";
    // throw code;
    return VK_FALSE;
}

const vk::ApplicationInfo APP_INFO
    = vk::ApplicationInfo{"egg", VK_MAKE_VERSION(0, 0, 0), "egg", VK_MAKE_VERSION(0, 0, 0), VK_API_VERSION_1_3};

renderer::renderer(GLFWwindow* window, flecs::world& world, std::unique_ptr<render_pipeline> pipeline) {
    // create Vulkan instance
    uint32_t                 glfw_ext_count = 0;
    auto*                    glfw_req_exts  = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
    std::vector<const char*> extensions{glfw_req_exts, glfw_req_exts + glfw_ext_count};
#ifndef NDEBUG
    extensions.emplace_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif
    instance = vk::createInstanceUnique(vk::InstanceCreateInfo{
        {}, &APP_INFO, 0, {}, (uint32_t)extensions.size(), extensions.data()});

    // set up vulkan debugging reports
#ifndef NDEBUG
    auto cbco = vk::DebugReportCallbackCreateInfoEXT{
        vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eDebug
            | vk::DebugReportFlagBitsEXT::ePerformanceWarning | vk::DebugReportFlagBitsEXT::eWarning,
        debug_callback};
    instance->createDebugReportCallbackEXT(
        &cbco, nullptr, &debug_report_callback, vk::DispatchLoaderDynamic(instance.get(), vkGetInstanceProcAddr)
    );
#endif
    // create window surface
    VkSurfaceKHR surface;

    auto err = glfwCreateWindowSurface(instance.get(), window, nullptr, &surface);
    if(err != VK_SUCCESS) throw std::runtime_error("failed to create window surface " + std::to_string(err));

    window_surface = vk::UniqueSurfaceKHR(surface);

    init_device(instance.get());

    // create different rendering layers
    fr = new frame_renderer(this);
    ir = new imgui_renderer();
    sr = new scene_renderer(world, std::move(pipeline));
}

renderer::~renderer() {
    delete sr;
    delete ir;
    delete fr;
}

void renderer::resize() { fr->reset_swapchain(); }

void renderer::render_frame() {
    auto frame = fr->begin_frame();
    ir->render_frame(frame);
    sr->render_frame(frame);
    fr->end_frame(std::move(frame));
}
