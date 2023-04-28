#include "egg/components.h"
#include <glm/gtx/io.hpp>
#include <iostream>

void comp::gpu_transform::update(const position& p, const rotation& r, const std::optional<mat4>& s)
    const {
    *transform = glm::translate(s.has_value() ? s.value() : mat4(1), p.pos) * glm::toMat4(r.rot);
}

void comp::camera::update(float aspect_ratio) const {
    *proj_transform.first = glm::perspective(fov, aspect_ratio, 0.f, 1000.f);
    std::cout << *proj_transform.first << "\n";
}
