#include "egg/components.h"
#include "egg/renderer/renderer.h"
#include "imgui.h"
#include <GLFW/glfw3.h>
#include <flecs.h>
#include <iostream>
#include "egg/input/input_distributor.h"
#include "egg/input/camera_interaction_models.h"

template<typename C>
class component_gui {
    std::shared_ptr<flecs::world> world;
    flecs::query<C> comps;
    const char* window_title;

    virtual bool editor(C* li, flecs::entity e) = 0;
public:
    component_gui(const std::shared_ptr<flecs::world>& world, const char* window_title)
        : world(world), comps(world->query<C>()), window_title(window_title)
    { }

    void window(bool* open) {
        if(ImGui::Begin(window_title, open)) {
            world->defer_begin();
            comps.iter([this](flecs::iter& it, C* components) {
                for(auto i : it) {
                    ImGui::PushID(i);
                    auto* c = &components[i];
                    if(editor(c, it.entity(i)))
                        it.entity(i).modified<C>();
                    ImGui::Separator();
                    ImGui::PopID();
                }
            });
            world->defer_end();
            if(ImGui::Button("Add...")) {
                auto l = world->entity();
                l.set<C>(C{});
            }
        }
        ImGui::End();
    }
};

class light_gui : public component_gui<comp::light> {
    // returns true if the light was modified
    bool editor(comp::light* li, flecs::entity _e) override {
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
        return mod;
    }
public:
    light_gui(const std::shared_ptr<flecs::world>& world)
        : component_gui(world, "Lights")
    { }
};

class camera_gui : public component_gui<comp::camera> {
    bool editor(comp::camera* cam_comp, flecs::entity cam) override {
        // TODO: just make a {positions,rotations}_gui?
        vec3 pos = cam.get<comp::position>()->pos;
        if(ImGui::DragFloat3("Position", &pos[0], 0.05f, -1000.f, 1000.f))
            cam.set<comp::position>({pos});
        quat rot = cam.get<comp::rotation>()->rot;
        if(ImGui::DragFloat4("Rotation (qut)", &rot[0], 0.05f, -1.f, 1.f))
            cam.set<comp::rotation>({glm::normalize(rot)});
        vec3 a = glm::eulerAngles(rot);
        if(ImGui::DragFloat3("Rotation (eul)", &a[0], 0.05f, 0.f, 2*glm::pi<float>()))
            cam.set<comp::rotation>({glm::quat(a)});
        bool mod = ImGui::DragFloat("Field of View", &cam_comp->fov, 0.1f, glm::pi<float>()/16.f, glm::pi<float>());
        return mod;
    }
public:
    camera_gui(const std::shared_ptr<flecs::world>& world)
        : component_gui(world, "Cameras")
    { }
};

void create_scene(asset_bundle* bndl, flecs::world* world, input_distributor* inp) {
    auto building_group = bndl->group_by_name("building").value();
    auto obs_group = bndl->group_by_name("obs").value();

    // instantiate the building group
    for(auto oi = bndl->group_objects(building_group); oi.has_more(); ++oi) {
        std::cout << bndl->string(bndl->object_name(*oi)) << "\n";
        auto e = world->entity();
        e.set<comp::renderable>(comp::renderable{*oi});
        e.set<comp::position>({});
        e.set<comp::rotation>({});
    }

    {
        auto oi = bndl->group_objects(obs_group);
        {
            auto e = world->entity();
            e.set<comp::renderable>(comp::renderable{*oi}); ++oi;
            e.set<comp::position>({6.f, -2.5f, 5.3f});
            e.set<comp::rotation>({});
            e.add<tag::tumble>();
        }
        assert(oi.has_more());
        {
            auto e = world->entity();
            e.set<comp::renderable>(comp::renderable{*oi}); ++oi;
            e.set<comp::position>({-6.f, -9.5f, -3.5f});
            e.set<comp::rotation>({});
            e.add<tag::tumble>();
        }
    }

    // add lights
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

    // create the camera
    auto cam = world->entity();
    cam.set<comp::position>({vec3(-8.f, 30.f, 22.f)});
    cam.set<comp::rotation>({
        quat{0.859f, -0.484f, -0.106f, 0.138f}
        //quat{0.f, 0.f, 0.897f, -0.443f}
    });
    cam.add<tag::active_camera>();
    cam.set<comp::camera>({});
    auto cam_inter = std::dynamic_pointer_cast<interaction_model>(std::make_shared<fly_camera_interaction_model>(5.0f));
    cam_inter->register_with_distributor(inp);
    cam.set<comp::interactable>(comp::interactable{ .active = true, .model = cam_inter });
}

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

    flecs::log::set_level(0);
    auto world = std::make_shared<flecs::world>();
    world->set<flecs::Rest>({});
    world->system<comp::rotation, tag::tumble>("tumble")
        .each([](flecs::iter& it, size_t row, comp::rotation& r, tag::tumble _t) {
            r.rot *= quat{vec3{
                .2f,
                .25f,
                .15f
            }*it.delta_time()};
            it.entity(row).modified<comp::rotation>();
        });

    auto* rndr = new renderer{window, world, argv[2]};

    auto bndl = std::make_shared<asset_bundle>(argv[1]);
    std::cout << "# groups = " << bndl->num_groups() << " " << "# objects = " << bndl->num_objects() << "\n";
    rndr->start_resource_upload(bndl);

    auto* inp = new input_distributor(window, rndr, *world);

    create_scene(bndl.get(), world.get(), inp);

    auto cg = camera_gui(world);
    rndr->imgui()->add_window("Cameras", [&](bool* open) { cg.window(open); });

    auto lg = light_gui(world);
    rndr->imgui()->add_window("Lights", [&](bool* open) { lg.window(open); });

    world->progress();

    rndr->wait_for_resource_upload_to_finish();
    while(glfwWindowShouldClose(window) == 0) {
        rndr->render_frame();
        glfwSwapBuffers(window);
        glfwPollEvents();
        world->progress();
    }

    delete inp;
    delete rndr;
    world.reset();
    glfwTerminate();
    return 0;
}
