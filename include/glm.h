#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_SWIZZLE
#define GLM_FORCE_RADIANS
#define GLM_FORCE_RIGHT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

using glm::mat4;
using glm::quat;
using glm::vec2;
using glm::vec3;
using glm::vec4;

struct aabb {
    vec3 min, max;

    void extend(const aabb& other) {
        this->min = glm::min(this->min, other.min);
        this->max = glm::max(this->max, other.max);
    }

    aabb transformed(const mat4& t) const {
        return aabb {
            .min = (t * vec4(this->min, 1.f)).xyz(),
            .max = (t * vec4(this->max, 1.f)).xyz(),
        };
    }

    vec3 extents() const {
        return (this->max - this->min) / 2.f;
    }
};
