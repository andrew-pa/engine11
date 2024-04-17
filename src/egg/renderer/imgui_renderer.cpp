#include "egg/renderer/imgui_renderer.h"
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

imgui_renderer::imgui_renderer(renderer* r, GLFWwindow* window) : r(r) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    float xsc, ysc;
    glfwGetWindowContentScale(window, &xsc, &ysc);
    ImGui::GetStyle().ScaleAllSizes(std::max(xsc, ysc));

    vk::DescriptorPoolSize pool_sizes[]
        = {vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 256)};

    desc_pool = r->dev->createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo{
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1024, 1, pool_sizes
    });

    image_sampler = r->dev->createSamplerUnique(vk::SamplerCreateInfo{
        {},
        vk::Filter::eNearest,
        vk::Filter::eNearest,
        vk::SamplerMipmapMode::eNearest,
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge,
        vk::SamplerAddressMode::eClampToEdge,
        0.f,
        VK_FALSE,
        16.f
    });

    // create swapchain independent create info for render pass
    vk::AttachmentDescription attachments[]{
        {vk::AttachmentDescriptionFlags(), // swapchain color
         r->surface_format.format,
         vk::SampleCountFlagBits::e1,
         vk::AttachmentLoadOp::eLoad,
         vk::AttachmentStoreOp::eStore,
         vk::AttachmentLoadOp::eDontCare,
         vk::AttachmentStoreOp::eDontCare,
         vk::ImageLayout::ePresentSrcKHR,
         vk::ImageLayout::ePresentSrcKHR},
    };

    vk::AttachmentReference refs[]{
        {0, vk::ImageLayout::eColorAttachmentOptimal},
    };

    vk::SubpassDescription subpasses[]{
        {vk::SubpassDescriptionFlags(),
         vk::PipelineBindPoint::eGraphics,
         0, nullptr,
         1, refs,
         nullptr, nullptr}
    };

    vk::SubpassDependency depds[]{
        {VK_SUBPASS_EXTERNAL,
         0, vk::PipelineStageFlagBits::eColorAttachmentOutput,
         vk::PipelineStageFlagBits::eColorAttachmentOutput,
         vk::AccessFlagBits::eColorAttachmentRead,
         vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite},
    };

    // create render pass
    render_pass       = r->dev->createRenderPassUnique(vk::RenderPassCreateInfo{
        vk::RenderPassCreateFlags(), 1, attachments, 1, subpasses, 1, depds
    });
    start_render_pass = vk::RenderPassBeginInfo{render_pass.get()};

    ImGui_ImplGlfw_InitForVulkan(window, false);
    ImGui_ImplVulkan_InitInfo imvk_init_info = {
        .Instance        = r->instance.get(),
        .PhysicalDevice  = r->phy_dev,
        .Device          = r->dev.get(),
        .QueueFamily     = r->graphics_queue_family_index,
        .Queue           = r->graphics_queue,
        .PipelineCache   = VK_NULL_HANDLE,
        .DescriptorPool  = desc_pool.get(),
        .Subpass         = 0,
        .MinImageCount   = r->surface_image_count,
        .ImageCount      = r->surface_image_count,
        .Allocator       = nullptr,
        .CheckVkResultFn = nullptr,
    };
    ImGui_ImplVulkan_Init(&imvk_init_info, render_pass.get());

    add_window("ImGui Demo", ImGui::ShowDemoWindow);
    add_window("ImGui Metrics", ImGui::ShowMetricsWindow);
}

imgui_renderer::~imgui_renderer() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void imgui_renderer::create_swapchain_depd(frame_renderer* fr) {
    framebuffers = fr->create_framebuffers(render_pass.get());
    start_render_pass.setRenderArea(vk::Rect2D(vk::Offset2D(), fr->get_current_extent()));
}

void imgui_renderer::start_resource_upload(vk::CommandBuffer upload_cmds) {
    ImGui_ImplVulkan_CreateFontsTexture(upload_cmds);
}

void imgui_renderer::resource_upload_cleanup() { ImGui_ImplVulkan_DestroyFontUploadObjects(); }

void imgui_renderer::render_frame(frame& frame) {
    start_render_pass.setFramebuffer(framebuffers[frame.index].get());
    frame.cmd_buf.beginRenderPass(start_render_pass, vk::SubpassContents::eInline);
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if(ImGui::BeginPopupContextVoid("#mainmenu")) {
        if(ImGui::BeginMenu("Windows")) {
            for(auto& [name, window] : windows)
                ImGui::MenuItem(name.c_str(), nullptr, &window.second);
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    for(auto& [name, window] : windows)
        if(window.second) window.first(&window.second);

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frame.cmd_buf);
    frame.cmd_buf.endRenderPass();
}

void imgui_renderer::add_window(const std::string& name, const std::function<void(bool*)>& draw) {
    windows[name] = {draw, false};
}

uint64_t imgui_renderer::add_texture(vk::ImageView image_view, vk::ImageLayout image_layout) {
    return (uint64_t
    )ImGui_ImplVulkan_AddTexture(image_sampler.get(), image_view, (VkImageLayout)image_layout);
}
