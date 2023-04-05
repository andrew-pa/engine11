#include "egg/renderer/renderer.h"
#include <GLFW/glfw3.h>
#include <flecs.h>
#include <iostream>

int main(int argc, char* argv[]) {
    GLFWwindow* window;

    /* Initialize the library */
    if(glfwInit() == 0) return -1;

    // disable GLFW setting up an OpenGL swapchain
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // Create a windowed mode window
    window = glfwCreateWindow(1280, 960, "erg", nullptr, nullptr);
    if(window == nullptr) {
        glfwTerminate();
        return -1;
    }

    flecs::world world;

    renderer rndr{window, world, nullptr};

    /* Loop until the user closes the window */
    while(glfwWindowShouldClose(window) == 0) {
        world.progress();
        rndr.render_frame();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
