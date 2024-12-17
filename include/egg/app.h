#pragma once
#include "bundle.h"
#include "input/input_distributor.h"
#include "renderer/renderer.h"
#include <GLFW/glfw3.h>
#include <flecs.h>
#include <memory>
#include <string_view>

class app {
    GLFWwindow* window;

  protected:
    std::shared_ptr<flecs::world>      world;
    std::unique_ptr<renderer>          rndr;
    std::shared_ptr<asset_bundle>      assets;
    std::unique_ptr<input_distributor> inpd;

    virtual std::unique_ptr<rendering_algorithm> create_rendering_algorithm() = 0;
    virtual std::shared_ptr<asset_bundle>        load_assets()                = 0;
    virtual void                                 create_scene()               = 0;

  public:
    app() = default;
    ~app();

    void init(std::string_view window_title);
    void run();
};
