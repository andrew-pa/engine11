#include "egg/app.h"
#include <stdexcept>

void app::init(std::string_view window_title) {
    if(glfwInit() == 0) throw std::runtime_error("failed to initialize GLFW");

    // disable GLFW setting up an OpenGL swapchain
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(1280, 960, window_title.data(), nullptr, nullptr);
    if(window == nullptr) {
        glfwTerminate();
        throw std::runtime_error("failed to create GLFW window");
    }

    world = std::make_shared<flecs::world>();
    world->set<flecs::Rest>({});

    rndr   = std::make_shared<renderer>(window, world, argv[2]);
    assets = load_assets();
    rndr->start_resource_upload(assets);

    inpd = std::make_unique<input_distributor>(window, rndr.get(), *world);

    create_scene();
}

void app::run() {
    world->progress();

    rndr->wait_for_resource_upload_to_finish();
    while(glfwWindowShouldClose(window) == 0) {
        rndr->render_frame();
        glfwSwapBuffers(window);
        glfwPollEvents();
        world->progress();
    }
}

app::~app() {
    inpd.reset();
    rndr.reset();
    world.reset();
    glfwTerminate();
}
