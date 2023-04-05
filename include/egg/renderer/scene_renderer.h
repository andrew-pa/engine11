#pragma once
#include "egg/renderer/core/frame_renderer.h"
#include "renderer.h"

class scene_renderer {
  public:
    scene_renderer(flecs::world& world, std::unique_ptr<render_pipeline> pipeline);
    void render_frame(frame& frame);
};
