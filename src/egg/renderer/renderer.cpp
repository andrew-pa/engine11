#include "egg/renderer/renderer.h"
#include "asset-bundler/format.h"
#include "egg/renderer/core/frame_renderer.h"
#include "egg/renderer/imgui_renderer.h"
#include "egg/renderer/memory.h"
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

const vk::ApplicationInfo APP_INFO = vk::ApplicationInfo{
    "egg", VK_MAKE_VERSION(0, 0, 0), "egg", VK_MAKE_VERSION(0, 0, 0), VK_API_VERSION_1_3};

vk::Extent2D get_window_extent(GLFWwindow* window) {
    vk::Extent2D e;
    glfwGetWindowSize(window, (int*)&e.width, (int*)&e.height);
    return e;
}

renderer::renderer(
    GLFWwindow*                          window,
    std::shared_ptr<flecs::world>        world,
    std::unique_ptr<rendering_algorithm> pipeline
) {
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
            | vk::DebugReportFlagBitsEXT::ePerformanceWarning | vk::DebugReportFlagBitsEXT::eWarning
            | vk::DebugReportFlagBitsEXT::eInformation,
        debug_callback};
    instance->createDebugReportCallbackEXT(
        &cbco,
        nullptr,
        &debug_report_callback,
        vk::DispatchLoaderDynamic(instance.get(), vkGetInstanceProcAddr)
    );
#endif
    // create window surface
    VkSurfaceKHR surface;

    auto err = glfwCreateWindowSurface(instance.get(), window, nullptr, &surface);
    if(err != VK_SUCCESS) {
        for(const auto* ext : extensions)
            std::cout << "requested extension: " << ext << "\n";
        throw std::runtime_error(
            "failed to create window surface " + vk::to_string(vk::Result(err))
        );
    }

    window_surface = vk::UniqueSurfaceKHR(surface);

    init_device(instance.get());

    command_pool = dev->createCommandPoolUnique(vk::CommandPoolCreateInfo{
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer, graphics_queue_family_index});

    // read surface capabilities, which should be stable throughout execution
    auto surface_caps   = phy_dev.getSurfaceCapabilitiesKHR(window_surface.get());
    surface_image_count = std::max(surface_caps.minImageCount, 2u);
    std::cout << "using a swap chain with " << surface_image_count << " images\n";

    auto fmts = phy_dev.getSurfaceFormatsKHR(window_surface.get());
    for(auto fmt : fmts)
        std::cout << "available format " << vk::to_string(fmt.format) << " / "
                  << vk::to_string(fmt.colorSpace) << "\n";
    surface_format = fmts[0];

    glfwSetWindowUserPointer(window, this);
    glfwSetWindowSizeCallback(window, [](GLFWwindow* wnd, int w, int h) {
        auto* r = (renderer*)glfwGetWindowUserPointer(wnd);
        r->resize(wnd);
    });

    // create different rendering layers
    fr = new frame_renderer(this, get_window_extent(window));
    ir = new imgui_renderer(this, window);
    ir->create_swapchain_depd(fr);
    sr = new scene_renderer(this, std::move(world), std::move(pipeline));
    sr->create_swapchain_depd(fr);
}

renderer::~renderer() {
    graphics_queue.waitIdle();
    present_queue.waitIdle();
    delete sr;
    delete ir;
    delete fr;
}

void renderer::start_resource_upload(const std::shared_ptr<asset_bundle>& assets) {
    upload_cmds  = std::move(dev->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{
        command_pool.get(), vk::CommandBufferLevel::ePrimary, 1})[0]);
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
    if(err != vk::Result::eSuccess) {
        throw std::runtime_error(
            "failed to wait for resource upload: " + vk::to_string(vk::Result(err))
        );
    }

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
    auto frame = fr->begin_frame();
    sr->render_frame(frame);
    ir->render_frame(frame);
    fr->end_frame(std::move(frame));
}

void renderer::add_gui_window(const std::string& name, const std::function<void(bool*)>& draw) {
    ir->add_window(name, draw);
}
