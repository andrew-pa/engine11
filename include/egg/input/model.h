#pragma once
#include <flecs.h>
#include <memory>
#include <optional>
#include <unordered_map>

using map_id = uint8_t;

struct key_axis_mapping {
    uint32_t positive_key, negative_key;

    key_axis_mapping(uint32_t negative_key, uint32_t positive_key)
        : positive_key(positive_key), negative_key(negative_key) {}
};

enum class mouse_mapping_mode { normal, delta };

struct mouse_axis_mapping {
    uint32_t           axis;
    mouse_mapping_mode mode;
};

struct axis_mapping {
    // select x=0 or y=1 axis of mouse coordinates
    std::optional<mouse_axis_mapping> mouse = std::nullopt;
    std::optional<key_axis_mapping>   key   = std::nullopt;
    // TODO: might want to also allow a key style mapping for gamepad D-pads
    // select axis from GLFW_GAMEPAD_AXIS_*
    std::optional<uint32_t> gamepad = std::nullopt;
};

struct button_mapping {
    // GLFW_MOUSE_BUTTON_*
    std::optional<uint32_t> mouse = std::nullopt;
    // GLFW_KEY_*
    std::optional<uint32_t> key = std::nullopt;
    // GLFW_GAMEPAD_BUTTON_*
    std::optional<uint32_t> gamepad = std::nullopt;
};

struct mapped_input {
    std::unordered_map<map_id, float> axis_states;
    std::unordered_map<map_id, bool>  button_states;
};

class interaction_model {
  public:
    virtual void register_with_distributor(class input_distributor* dist)            = 0;
    virtual void process_input(const mapped_input& input, flecs::entity e, float dt) = 0;
    virtual ~interaction_model()                                                     = default;
};

namespace comp {
struct interactable {
    bool                               active;
    std::shared_ptr<interaction_model> model;
};
}  // namespace comp
