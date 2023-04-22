#include "egg/renderer/renderer.h"
#include <GLFW/glfw3.h>
#include <flecs.h>
#include <iostream>

class empty_rendering_algorithm : public rendering_algorithm {
  public:
    void create_static_objects(
        vk::Device device, vk::AttachmentDescription present_surface_attachment
    ) override {}

    vk::RenderPassBeginInfo* get_render_pass_begin_info(uint32_t frame_index) override {
        return nullptr;
    }

    void create_pipeline_layouts(
        vk::Device              device,
        vk::DescriptorSetLayout scene_data_desc_set_layout,
        vk::PushConstantRange   per_object_push_constants_range
    ) override {}

    void create_pipelines(vk::Device device) override {}

    void create_framebuffers(frame_renderer* fr) override {}

    void generate_commands(
        vk::CommandBuffer                                          cb,
        vk::DescriptorSet                                          scene_data_desc_set,
        std::function<void(vk::CommandBuffer, vk::PipelineLayout)> generate_draw_cmds
    ) override {}

    ~empty_rendering_algorithm() override = default;
};

int main(int argc, char* argv[]) {
    GLFWwindow* window;

    if(glfwInit() == 0) return -1;

    // disable GLFW setting up an OpenGL swapchain
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(1280, 960, "erg", nullptr, nullptr);
    if(window == nullptr) {
        glfwTerminate();
        return -1;
    }

    auto world = std::make_shared<flecs::world>();

    renderer rndr{window, world, std::make_unique<empty_rendering_algorithm>()};

    auto bndl = std::make_shared<asset_bundle>(argv[1]);
    rndr.start_resource_upload(bndl);

    world->progress();

    rndr.wait_for_resource_upload_to_finish();
    while(glfwWindowShouldClose(window) == 0) {
        rndr.render_frame();
        glfwSwapBuffers(window);
        glfwPollEvents();
        world->progress();
    }

    glfwTerminate();
    return 0;
}
