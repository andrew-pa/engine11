#pragma once
#include "backends/imgui_impl_vulkan.h"
#include "egg/renderer/core/frame_renderer.h"
#include "imgui.h"

class imgui_renderer : public abstract_imgui_renderer{
    renderer* r;

    vk::UniqueDescriptorPool           desc_pool;
    vk::UniqueRenderPass               render_pass;
    vk::RenderPassBeginInfo            start_render_pass;
    std::vector<vk::UniqueFramebuffer> framebuffers;

    std::unordered_map<std::string, std::pair<std::function<void(bool*)>, bool>> windows;

    vk::UniqueSampler image_sampler;

  public:
    imgui_renderer(renderer* r, GLFWwindow* window);
    ~imgui_renderer();

    void create_swapchain_depd(frame_renderer* fr);

    void add_window(const std::string& name, const std::function<void(bool*)>& draw) override;

    uint64_t add_texture(vk::ImageView image_view, vk::ImageLayout image_layout) override;

    void start_resource_upload(vk::CommandBuffer upload_cmds);
    void resource_upload_cleanup();

    void render_frame(frame& frame);
};
