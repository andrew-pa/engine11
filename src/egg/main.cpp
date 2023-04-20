#include "egg/renderer/renderer.h"
#include <GLFW/glfw3.h>
#include <flecs.h>
#include <iostream>

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

    flecs::world world;

    renderer rndr{window, world, nullptr};

    auto bndl = std::make_shared<asset_bundle>(argv[1]);
    rndr.start_resource_upload(bndl);

    world.progress();

    rndr.wait_for_resource_upload_to_finish();
    while(glfwWindowShouldClose(window) == 0) {
        rndr.render_frame();
        glfwSwapBuffers(window);
        glfwPollEvents();
        world.progress();
    }

    glfwTerminate();
    return 0;
}
