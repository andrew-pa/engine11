#pragma once
#include "egg/components.h"
#include "imgui.h"
#include <memory>

template<typename C>
class component_gui {
    std::shared_ptr<flecs::world> world;
    flecs::query<C>               comps;
    const char*                   window_title;

    virtual bool editor(flecs::field<C> li, flecs::entity e) = 0;

  public:
    component_gui(const std::shared_ptr<flecs::world>& world, const char* window_title)
        : world(world), comps(world->query<C>()), window_title(window_title) {}

    void window(bool* open) {
        if(ImGui::Begin(window_title, open)) {
            world->defer_begin();
            comps.run([this](flecs::iter& it) {
                while(it.next()) {
                    auto i = it.id(0);
                    auto c = it.field<C>(0);
                    ImGui::PushID(i);
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
    bool editor(flecs::field<comp::camera> cam_comp, flecs::entity cam) override {
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
