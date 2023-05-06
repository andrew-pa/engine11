#include "egg/components.h"
#include "egg/renderer/algorithms/forward.h"
#include "egg/renderer/renderer.h"
#include "imgui.h"
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

    renderer* rndr = new renderer{window, world, std::make_unique<forward_rendering_algorithm>()};

    auto bndl = std::make_shared<asset_bundle>(argv[1]);
    rndr->start_resource_upload(bndl);

    // instantiate a group
    for(auto oi = bndl->group_objects(0); oi.has_more(); ++oi) {
        std::cout << bndl->string(bndl->object_name(*oi)) << "\n";
        auto e = world->entity();
        e.set<comp::renderable>(comp::renderable{*oi});
        e.set<comp::position>({});
        e.set<comp::rotation>({ quat{0.f, 0.f, 0.f, 1.f} });
    }

    // create the camera
    std::cout << "camera\n";
    auto cam = world->entity();
    cam.set<comp::position>({vec3(0.f, 0.f, 40.f)});
    cam.set<comp::rotation>({
        quat{0.f, 0.f, 0.897f, -0.443f}
    });
    cam.add<tag::active_camera>();
    cam.add<comp::camera>();

    rndr->imgui()->add_window("Camera", [&](bool* open) {
        ImGui::Begin("Camera", open);
        vec3 pos = cam.get<comp::position>()->pos;
        if(ImGui::DragFloat3("Position", &pos[0], 0.05f, -1000.f, 1000.f))
            cam.set<comp::position>({pos});
        quat rot = cam.get<comp::rotation>()->rot;
        if(ImGui::DragFloat4("Rotation", &rot[0], 0.05f, -1.f, 1.f))
            cam.set<comp::rotation>({glm::normalize(rot)});
        ImGui::End();
    });

    world->progress();

    rndr->wait_for_resource_upload_to_finish();
    while(glfwWindowShouldClose(window) == 0) {
        rndr->render_frame();
        glfwSwapBuffers(window);
        glfwPollEvents();
        // auto t = world->time();
        // cam.set<comp::position>({vec3(0.f, 0.f, 0.f)});
        world->progress();
    }

    delete rndr;
    world.reset();
    glfwTerminate();
    return 0;
}
