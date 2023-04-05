#include <iostream>
#include <GLFW/glfw3.h>
#include <flecs.h>
#include "egg/renderer/renderer.h"

int main(int argc, char* argv[]) {
    GLFWwindow* window;

    /* Initialize the library */
    if (glfwInit() == 0)
        return -1;

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(1280, 960, "erg", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    flecs::world world;

    renderer rndr{window, world, nullptr};

    /* Loop until the user closes the window */
    while (glfwWindowShouldClose(window) == 0) {
        world.progress();
        rndr.render_frame();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
