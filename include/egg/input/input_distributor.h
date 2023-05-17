#pragma once
#include <optional>
#include <unordered_map>
#include <GLFW/glfw3.h>
#include <flecs.h>
#include "egg/input/model.h"

class renderer;

class input_distributor {
	renderer* renderer;
	map_id next_id;
	std::unordered_map<const char*, map_id> id_names;
	std::unordered_map<map_id, axis_mapping> axises;
	std::unordered_map<map_id, button_mapping> buttons;

	mapped_input input_state[2];
	uint8_t current_input_state;
public:
	input_distributor(GLFWwindow* window, renderer* renderer, flecs::world& world);

	map_id register_axis_mapping(const char* name, axis_mapping mapping);
	map_id register_button_mapping(const char* name, button_mapping mapping);
	map_id id_for_name(const char* name);
};