#pragma once
#include "egg/input/model.h"
#include "glm.h"
#include <GLFW/glfw3.h>
#include <flecs.h>
#include <optional>
#include <unordered_map>

class renderer;

class input_distributor {
    // TODO: this is silly but we need to distribute the resize event to the renderer
    renderer*   r;
    GLFWwindow* window;
    vec2        window_size;

    map_id                                     next_id;
    std::unordered_map<const char*, map_id>    id_names;
    std::unordered_map<map_id, axis_mapping>   axises;
    std::unordered_map<map_id, button_mapping> buttons;

    vec2         last_mouse_pos;
    bool         mouse_enabled;
    mapped_input input_state;

    void init_callbacks(GLFWwindow* window);
    void init_ecs(flecs::world& world);

    void process_mouse_pos(vec2 p);
    void process_mouse_button(int button, int action);
    void process_key(int key, int action);

    void after_distribution();

    void build_gui(bool* open);

  public:
    input_distributor(GLFWwindow* window, renderer* r, flecs::world& world);

    map_id register_axis_mapping(const char* name, axis_mapping mapping);
    map_id register_button_mapping(const char* name, button_mapping mapping);
    map_id id_for_name(const char* name);
};
