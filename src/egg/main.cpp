#include "egg/components.h"
#include "egg/input/camera_interaction_models.h"
#include "egg/input/input_distributor.h"
#include "egg/renderer/renderer.h"
#include "glm.h"
#include "glm/ext/scalar_constants.hpp"
#include "glm/gtc/random.hpp"
#include "imgui.h"
#include <GLFW/glfw3.h>
#include <flecs.h>
#include <iostream>

template<typename C>
class component_gui {
    std::shared_ptr<flecs::world> world;
    flecs::query<C>               comps;
    const char*                   window_title;

    virtual bool editor(C* li, flecs::entity e) = 0;

  public:
    component_gui(const std::shared_ptr<flecs::world>& world, const char* window_title)
        : world(world), comps(world->query<C>()), window_title(window_title) {}

    void window(bool* open) {
        if(ImGui::Begin(window_title, open)) {
            world->defer_begin();
            comps.iter([this](flecs::iter& it, C* components) {
                for(auto i : it) {
                    ImGui::PushID(i);
                    auto* c = &components[i];
                    if(editor(c, it.entity(i))) it.entity(i).modified<C>();
                    ImGui::Separator();
                    ImGui::PopID();
                }
            });
            world->defer_end();
        }
        ImGui::End();
    }
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
        if(ImGui::DragFloat3("Rotation (eul)", &a[0], 0.05f, 0.f, 2 * glm::pi<float>()))
            cam.set<comp::rotation>({glm::quat(a)});
        bool mod = ImGui::DragFloat(
            "Field of View", &cam_comp->fov, 0.1f, glm::pi<float>() / 16.f, glm::pi<float>()
        );
        return mod;
    }

  public:
    camera_gui(const std::shared_ptr<flecs::world>& world) : component_gui(world, "Cameras") {}
};

struct wander {
    aabb  bounds;
    vec3  vel;
    float time_to_next_direction_change, timer, speed;

    wander(aabb bounds = {}, float ttndc = 0.f, float speed = 5.f)
        : bounds(bounds), vel(glm::sphericalRand(speed)), time_to_next_direction_change(ttndc),
          timer(ttndc), speed(speed) {}
};

void create_scene(asset_bundle* bndl, flecs::world* world, input_distributor* inp) {
    auto building_group = bndl->group_by_name("building").value();
    auto obs_group      = bndl->group_by_name("obs").value();

    aabb world_bounds = bndl->group_bounds(building_group);

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
            e.set<comp::renderable>(comp::renderable{*oi});
            ++oi;
            e.set<comp::position>({-6.f, -5.3f, 2.5f});
            e.set<comp::rotation>({quat{vec3{0.5f, 0.f, 0.5f}}});
            e.set<comp::tumble>({0.5f});
        }
        assert(oi.has_more());
        {
            auto e = world->entity();
            e.set<comp::renderable>(comp::renderable{*oi});
            ++oi;
            e.set<comp::position>({6.f, 3.5f, 9.5f});
            e.set<comp::rotation>({});
            e.set<comp::tumble>({0.25f});
        }
    }

    // add lights
    std::array<vec3, 3> light_colors = {
        vec3(1.1f, 1.f, 0.8f) * 0.8f,
        vec3(0.8f, 1.f, 1.1f) * 0.1f,
        vec3(1.1f, 0.8f, 1.f) * 0.1f,
    };

    for(size_t i = 0; i < 3; ++i) {
        float t = (float)i / 3.f;
        auto  l = world->entity();
        l.set<comp::rotation>({quat{vec3{0.f, t * 2.f * glm::pi<float>(), 0.f}}});
        l.add<comp::directional_light>();
        l.set<comp::light>({light_colors[i]});
    }

    for(size_t i = 0; i < 2; ++i) {
        auto l = world->entity();
        l.set<comp::position>({vec3{glm::linearRand(world_bounds.min, world_bounds.max)}});
        l.set<comp::point_light>({8.0f});
        l.set<comp::light>({glm::linearRand(vec3(0.3f), vec3(6.f)) * 10.f});
        l.set<wander>({world_bounds, 20.f, 4.f});
    }

    // create the camera
    auto cam = world->entity();
    cam.set<comp::position>({vec3(-8.f, 30.f, 22.f)});
    cam.set<comp::rotation>({
        quat{0.859f, -0.484f, -0.106f, 0.138f}
    });
    cam.add<tag::active_camera>();
    cam.set<comp::camera>({});
    auto cam_inter = std::dynamic_pointer_cast<interaction_model>(
        std::make_shared<fly_camera_interaction_model>(5.0f)
    );
    cam_inter->register_with_distributor(inp);
    cam.set<comp::interactable>(comp::interactable{.active = true, .model = cam_inter});
}

int main(int argc, char* argv[]) {
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

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
    world->system<comp::rotation, const comp::tumble>("tumble").each(
        [](flecs::iter& it, size_t row, comp::rotation& r, const comp::tumble t) {
            r.rot *= quat{
                vec3{1.0f, 1.25f, .75f}
                * t.speed * it.delta_time()
            };
            it.entity(row).modified<comp::rotation>();
        }
    );
    world->system<comp::position, wander>("wander").each(
        [](flecs::iter& it, size_t row, comp::position& p, wander& w) {
            if(!w.bounds.contains(p.pos)) w.vel *= -1.f;
            p.pos += w.vel * it.delta_time();
            w.timer -= it.delta_time();
            if(w.timer <= 0) {
                w.timer = w.time_to_next_direction_change;
                w.vel   = glm::sphericalRand(w.speed);
            }
            it.entity(row).modified<comp::position>();
        }
    );

    auto* rndr = new renderer{window, world, argv[2]};

    auto bndl = std::make_shared<asset_bundle>(argv[1]);
    std::cout << "# groups = " << bndl->num_groups() << " "
              << "# objects = " << bndl->num_objects() << "\n";
    rndr->start_resource_upload(bndl);

    auto* inp = new input_distributor(window, rndr, *world);

    create_scene(bndl.get(), world.get(), inp);

    auto cg = camera_gui(world);
    rndr->imgui()->add_window("Cameras", [&](bool* open) { cg.window(open); });

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

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
