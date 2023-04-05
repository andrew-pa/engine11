#include "egg/renderer/renderer.h"
#include "egg/renderer/frame_renderer.h"
#include "egg/renderer/imgui_renderer.h"
#include "egg/renderer/scene_renderer.h"

renderer::renderer(
        GLFWwindow* window,
        flecs::world& world,
        std::unique_ptr<render_pipeline> pipeline)
{
    fr = new frame_renderer(window);
    ir = new imgui_renderer();
    sr = new scene_renderer(world, std::move(pipeline));
}

renderer::~renderer() {
    delete sr;
    delete ir;
    delete fr;
}

void renderer::render_frame() {
    auto frame = fr->begin_frame();
    ir->render_frame(frame);
    sr->render_frame(frame);
    fr->end_frame(std::move(frame));
}

