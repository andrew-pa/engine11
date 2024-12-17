#include "egg/components.h"
#include "egg/input/camera_interaction_models.h"
#include "egg/input/input_distributor.h"
#include "egg/renderer/renderer.h"
#include "glm.h"
#include "glm/ext/scalar_constants.hpp"
#include "glm/gtc/random.hpp"
#include <GLFW/glfw3.h>
#include <flecs.h>
#include <iostream>
#include <utility>
#include "egg/components_gui.h"
#include "egg/app.h"
#include "egg/renderer/algorithms/forward.h"

struct wander {
    aabb  bounds;
    vec3  vel;
    float time_to_next_direction_change, timer, speed;

    wander(aabb bounds = {}, float ttndc = 0.f, float speed = 5.f)
        : bounds(bounds), vel(glm::sphericalRand(speed)), time_to_next_direction_change(ttndc),
          timer(ttndc), speed(speed) {}
};

class demo_app : public app {
    std::filesystem::path assets_path;
protected:
    std::shared_ptr<asset_bundle> load_assets() override {
        return std::make_shared<asset_bundle>(assets_path);
    }

    std::unique_ptr<rendering_algorithm> create_rendering_algorithm() override {
        return std::make_unique<forward_rendering_algorithm>();
    }

    void setup_ecs_systems() {
        world->system<comp::rotation, const comp::tumble>("tumble").each(
            [](flecs::iter& it, size_t row, comp::rotation& r, const comp::tumble t) {
                r.rot *= quat{
                    vec3{1.0f, 1.25f, .75f} * t.speed * it.delta_time()
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

        auto cg = camera_gui(world);
        rndr->imgui()->add_window("Cameras", [&](bool* open) { cg.window(open); });
    }

    void create_scene() override {
        setup_ecs_systems();

        auto building_group = assets->group_by_name("building").value();
        auto obs_group      = assets->group_by_name("obs").value();

        aabb world_bounds = assets->group_bounds(building_group);

        // instantiate the building group
        for(auto oi = assets->group_objects(building_group); oi.has_more(); ++oi) {
            std::cout << assets->string(assets->object_name(*oi)) << "\n";
            auto e = world->entity();
            e.set<comp::renderable>(comp::renderable{*oi});
            e.set<comp::position>({});
            e.set<comp::rotation>({});
        }

        {
            auto oi = assets->group_objects(obs_group);
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
        cam_inter->register_with_distributor(inpd.get());
        cam.set<comp::interactable>(comp::interactable{.active = true, .model = cam_inter});
    }
public:
    demo_app(std::filesystem::path assets_path) : assets_path(std::move(assets_path)) {}
};



int main(int argc, char* argv[]) {
    flecs::log::set_level(0);

    demo_app app(argv[1]);
    try {
        app.init("engine11 demo");
    } catch(std::runtime_error e) {
        std::cerr << "failed to initalize app: " << e.what() << "\n";
        return -1;
    }

    app.run();

    return 0;
}
