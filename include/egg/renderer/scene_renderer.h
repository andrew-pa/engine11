#pragma once
#include "renderer.h"
#include "frame_renderer.h"

class scene_renderer {
public:
    scene_renderer(flecs::world& world, std::unique_ptr<render_pipeline> pipeline);
    void render_frame(frame& frame);
};
