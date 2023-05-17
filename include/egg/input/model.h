#pragma once
#include <optional>
#include <unordered_map>
#include <memory>
#include <flecs.h>

using map_id = uint8_t;

struct key_axis_mapping {
	uint32_t positive_key, negative_key;
};

struct axis_mapping {
	// select x=0 or y=1 axis of mouse coordinates
	std::optional<uint32_t> mouse = std::nullopt;
	std::optional<key_axis_mapping> key = std::nullopt;
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
	std::unordered_map<map_id, bool> button_states;
};

class interaction_model {
public:
	virtual void process_input(const mapped_input& input, flecs::entity e) = 0;
	virtual ~interaction_model() = default;
};

namespace comp {
	struct interactable {
		std::shared_ptr<interaction_model> model;
	};
}
