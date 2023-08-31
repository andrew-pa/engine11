#include "egg/components.h"
#include "egg/renderer/algorithms/forward.h"
#include "egg/renderer/renderer.h"
#include "imgui.h"
#include <GLFW/glfw3.h>
#include <flecs.h>
#include <iostream>
#include "egg/input/input_distributor.h"
#include "egg/input/camera_interaction_models.h"

int main(int argc, char* argv[]) {
    GLFWwindow* window;

    if(glfwInit() == 0) return -1;

    // disable GLFW setting up an OpenGL swapchain
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(1280, 960, "engine11", nullptr, nullptr);
    if(window == nullptr) {
        glfwTerminate();
        return -1;
    }

    auto world = std::make_shared<flecs::world>();
    world->set<flecs::Rest>({});

    auto* rndr = new renderer{window, world, argv[2]};

    auto bndl = std::make_shared<asset_bundle>(argv[1]);
    rndr->start_resource_upload(bndl);

    auto* inp = new input_distributor(window, rndr, *world);

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
        quat{0.f, 0.f, 0.f, 1.f}
        //quat{0.f, 0.f, 0.897f, -0.443f}
    });
    cam.add<tag::active_camera>();
    cam.set<comp::camera>({});
    auto cam_inter = std::dynamic_pointer_cast<interaction_model>(std::make_shared<fly_camera_interaction_model>(5.0f));
    cam_inter->register_with_distributor(inp);
    cam.set<comp::interactable>(comp::interactable{ .active = true, .model = cam_inter });

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

    auto lgh1 = world->entity();
    auto lgh = comp::light{
        comp::light_info {
            .emmitance = vec3(1.1f, 1.f, 0.9f) * 2.f,
            .type = comp::light_type::directional,
            .position = vec3(0.f),
            .param1 = 0.f,
            .direction = normalize(vec3(0.1f, 0.45f, 1.f)),
            .param2 = 0.f
        }
    };
    lgh1.set<comp::light>(lgh);

    /*auto lgh2 = world->entity();
    lgh2.set<comp::light>(comp::light{
        comp::light_info {
            .emmitance = vec3(4.5f, 5.f, 5.5f),
            .type = comp::light_type::directional,
            .position = vec3(0.f),
            .param1 = 0.f,
            .direction = normalize(vec3(-0.1f, 0.35f, 1.f)),
            .param2 = 0.f
        }
    });

    auto lgh3 = world->entity();
    lgh3.set<comp::light>(comp::light{
        comp::light_info {
            .emmitance = vec3(1.f, 1.5f, 1.f),
            .type = comp::light_type::directional,
            .position = vec3(0.f),
            .param1 = 0.f,
            .direction = normalize(vec3(0.1f, 1.f, 0.2f)),
            .param2 = 0.f
        }
    });*/

    auto lgh4 = world->entity();
    lgh4.set<comp::light>(comp::light{
        comp::light_info {
            .emmitance = vec3(10.f, 12.0f, 18.f),
            .type = comp::light_type::spot,
            .position = vec3(0.f, 0.f, 10.f),
            .param1 = 0.9f,
            .direction = vec3(0.f, 0.f, 1.f),
            .param2 = 0.5f
        }
    });

    auto light_query = world->query<comp::light>();
    rndr->imgui()->add_window("Light", [&](bool* open) {
        if(ImGui::Begin("Light", open)) {
            world->defer_begin();
            light_query.iter([](flecs::iter& it, comp::light* lis) {
                for(auto i : it) {
                ImGui::PushID(i);
                auto* li = &lis[i];
                bool mod = false;
                if(ImGui::BeginCombo("Type", comp::light_type_str(li->info.type))) {
#define X(name, ltype) \
                    if(ImGui::Selectable(name, li->info.type == comp::light_type::ltype)) { \
                        li->info.type = comp::light_type::ltype; \
                        mod = true; \
                    }
                    X("Directional", directional);
                    X("Point", point);
                    X("Spot", spot);
#undef X
                    ImGui::EndCombo();
                }
                if(ImGui::ColorEdit3("Color", &li->info.emmitance[0], ImGuiColorEditFlags_Float|ImGuiColorEditFlags_HDR)) mod = true;
                if(ImGui::DragFloat3("Position", &li->info.position[0], 0.05f, -1000.f, 1000.f)) mod = true;
                if(ImGui::DragFloat3("Direction", &li->info.direction[0], 0.05f, -1000.f, 1000.f)) {
                    li->info.direction = glm::normalize(li->info.direction);
                    mod = true;
                }
                if(ImGui::DragFloat("Param 1", &li->info.param1, 0.005f, 0.f, 1000.f)) mod = true;
                if(ImGui::DragFloat("Param 2", &li->info.param2, 0.005f, 0.f, 1000.f)) mod = true;
                if(mod) it.entity(i).modified<comp::light>();
                ImGui::Separator();
                ImGui::PopID();
                }
            });
            world->defer_end();
            if(ImGui::Button("Add Light...")) {
                auto l = world->entity();
                l.set<comp::light>(comp::light{});
            }
        }
        ImGui::End();
    });

    // auto lgh4 = world->entity();
    // lgh4.set<comp::light>(comp::light{
    //     comp::light_info {
    //         .emmitance = vec3(0.f, 0.2f, 1.f),
    //         .type = comp::light_type::spot,
    //         .position = vec3(0.f, 30.f, 0.f),
    //         .param1 = 0.f,
    //         .direction = vec3(0.f, -1.f, 0.f),
    //         .param2 = 3.f
    //     }
    // });

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

    delete inp;
    delete rndr;
    world.reset();
    glfwTerminate();
    return 0;
}
