#include "egg/components.h"
#include <glm/gtx/io.hpp>
#include <iostream>

void comp::gpu_transform::update(const position& p, const rotation& r, const std::optional<mat4>& s)
    const {

    mat4 static_tf(1);
    if(s.has_value()) static_tf = s.value();
    *transform = glm::translate(static_tf, p.pos) * glm::mat4_cast(r.rot);
    // *transform = glm::translate(mat4(1), p.pos) * glm::mat4_cast(r.rot) * static_tf;
    //*transform =  static_tf * glm::mat4_cast(r.rot) * glm::translate(mat4(1), p.pos);
    // std::cout << "T" << (*transform) << "\n";
}

void comp::camera::update(float aspect_ratio) const {
    // it's important that the near plane != 0 apparently
    *proj_transform.first = glm::perspective(fov, aspect_ratio, 0.1f, 500.f);
    // std::cout << *proj_transform.first << "\n";
}

void comp::light::update(flecs::entity e) const {
    vec3 pos, dir;

    if(type == light_type::point || type == light_type::spot) pos = e.get<comp::position>()->pos;

    if(type == light_type::directional || type == light_type::spot) {
        auto r = e.get<comp::rotation>()->rot;
        dir    = glm::rotate(r, vec3(0.f, 0.f, 1.f));
    }

    if(gpu_info.first != nullptr)
        *gpu_info.first = light_info{
            .emmitance = this->emmitance,
            .type      = this->type,
            .position  = pos,
            .param1    = this->param1,
            .direction = dir,
            .param2    = this->param2,
        };
}
