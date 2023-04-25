#pragma once
#include "asset-bundler/format.h"
#include "glm/ext/scalar_constants.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <optional>

using glm::mat4;
using glm::quat;
using glm::vec3;

namespace comp {

struct position {
    vec3 pos;
};

struct rotation {
    quat rot;
};

struct gpu_transform {
    mat4*  transform;
    size_t gpu_index;

    void update(const position& p, const rotation& r, const std::optional<mat4>& s = {}) const;
};

struct camera {
    float                    fov = glm::pi<float>() / 4.f;
    std::pair<mat4*, size_t> proj_transform;

    void update(float aspect_ratio) const;
};

struct renderable {
    object_id object;
};

}  // namespace comp

namespace tag {
struct active_camera {};
}  // namespace tag
