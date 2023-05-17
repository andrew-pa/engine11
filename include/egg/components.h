#pragma once
#include "asset-bundler/format.h"
#include "glm.h"
#include <optional>

namespace comp {

struct position {
    vec3 pos;
};

struct rotation {
    quat rot;
    rotation(quat r = quat(0.f, 0.f, 0.f, 0.f)) : rot(r) {}
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

enum class light_type {
    directional = 1,
    point = 2,
    spot = 3
};

struct light_info {
    vec3 emmitance; light_type type;
    vec3 position; float param1;
    vec3 direction; float param2;
};

struct light {
    light_info info;
    std::pair<light_info*, size_t> gpu_info;

    void update() const;
};

}  // namespace comp

namespace tag {
struct active_camera {};
}  // namespace tag
