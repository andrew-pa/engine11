#include "egg/components.h"
#include <glm/gtx/io.hpp>
#include <iostream>

void comp::gpu_transform::update(const position& p, const rotation& r, const std::optional<mat4>& s)
    const {

    mat4 static_tf(1);
    if (s.has_value()) static_tf = s.value();
    *transform = glm::translate(static_tf, p.pos) * glm::mat4_cast(r.rot);
    // *transform = glm::translate(mat4(1), p.pos) * glm::mat4_cast(r.rot) * static_tf;
    //*transform =  static_tf * glm::mat4_cast(r.rot) * glm::translate(mat4(1), p.pos);
	//std::cout << "T" << (*transform) << "\n";
}

void comp::camera::update(float aspect_ratio) const {
    // it's important that the near plane != 0 apparently
    *proj_transform.first = glm::perspective(fov, aspect_ratio, 0.1f, 500.f);
    //std::cout << *proj_transform.first << "\n";
}

void comp::light::update() const {
    if(gpu_info.first != nullptr)
		*gpu_info.first = info;
}
