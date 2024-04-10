#pragma once
#include "asset-bundler/format.h"
#include "glm.h"
#include <flecs.h>
#include <optional>

namespace comp {

struct position {
    vec3 pos;

    position() : pos(0) {}

    position(float x, float y, float z) : pos(x, y, z) {}

    position(vec3 v) : pos(v) {}
};

struct rotation {
    quat rot;

    rotation(quat r = quat{1.f, 0.f, 0.f, 0.f}) : rot(r) {}
};

struct gpu_transform {
    mat4*  transform;
    size_t gpu_index;

    void update(const position& p, const rotation& r, const std::optional<mat4>& s) const;
};

struct camera {
    float                    fov = glm::pi<float>() / 4.f;
    std::pair<mat4*, size_t> proj_transform;

    void update(float aspect_ratio) const;
};

struct renderable {
    object_id object;
};

enum class light_type { directional = 1, point = 2, spot = 3 };

inline const char* light_type_str(light_type t) {
    switch(t) {
        case light_type::directional: return "Directional";
        case light_type::point: return "Point";
        case light_type::spot: return "Spot";
        default: return "unknown";
    }
}

struct light_info {
    vec3       emmitance;
    light_type type;
    vec3       position;
    float      param1;
    vec3       direction;
    float      param2;
};

struct light {
    std::pair<light_info*, size_t> gpu_info;
    light_type                     type;
    vec3                           emmitance;
    float                          param1, param2;

    light(
        light_type t = light_type::directional, vec3 e = vec3(0.f), float p1 = 0.f, float p2 = 0.f
    )
        : gpu_info{nullptr, 0}, type(t), emmitance(e), param1(p1), param2(p2) {}

    void update(flecs::entity e) const;
};

struct tumble {
    float speed;

    tumble(float speed = 1.f) : speed(speed) {}
};

}  // namespace comp

namespace tag {
struct active_camera {};

}  // namespace tag
