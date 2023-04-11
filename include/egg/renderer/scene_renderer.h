#pragma once
#include "egg/renderer/core/frame_renderer.h"
#include "renderer.h"

/*
 * transforms (can change once per frame)
 * materials (changes infrequently, technically static except editor)
 * textures (static if you load all assets up front)
 * geometry (static if you load all assets up front)
 */

class scene_renderer {
  public:
    scene_renderer(flecs::world& world, std::unique_ptr<render_pipeline> pipeline);
    void render_frame(frame& frame);
};
