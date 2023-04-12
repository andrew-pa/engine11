#include "egg/renderer/scene_renderer.h"

scene_renderer::scene_renderer(flecs::world& world, std::unique_ptr<render_pipeline> pipeline) {}

void scene_renderer::render_frame(frame& frame) {
    // frame.frame_cmd_buf.beginRenderPass({}, vk::SubpassContents::eSecondaryCommandBuffers);
    // frame.frame_cmd_buf.endRenderPass();
}
