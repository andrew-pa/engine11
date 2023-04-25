#include "egg/components.h"
#include "egg/renderer/algorithms/forward.h"
#include "egg/renderer/renderer.h"
#include "glm/ext/scalar_constants.hpp"
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

    auto world = std::make_shared<flecs::world>();

    renderer rndr{window, world, std::make_unique<forward_rendering_algorithm>(argv[2])};

    auto bndl = std::make_shared<asset_bundle>(argv[1]);
    rndr.start_resource_upload(bndl);

    // instantiate a group
    for(auto oi = bndl->group_objects(0); oi.has_more(); ++oi) {
        std::cout << bndl->string(bndl->object_name(*oi)) << "\n";
        auto e = world->entity();
        e.set<comp::position>({});
        e.set<comp::rotation>({});
        e.set<comp::renderable>(comp::renderable{*oi});
    }

    // create the camera
    std::cout << "camera\n";
    auto e = world->entity();
    e.set<comp::position>({vec3(0.f, 0.f, 0.f)});
    e.set<comp::rotation>({});
    e.add<tag::active_camera>();
    e.add<comp::camera>();

    world->progress();

    rndr.wait_for_resource_upload_to_finish();
    while(glfwWindowShouldClose(window) == 0) {
        rndr.render_frame();
        glfwSwapBuffers(window);
        glfwPollEvents();
        world->progress();
    }

    glfwTerminate();
    return 0;
}
