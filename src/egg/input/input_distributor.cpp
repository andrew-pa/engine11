#include "egg/input/input_distributor.h"
#include "egg/renderer/renderer.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"

input_distributor::input_distributor(GLFWwindow* window, renderer* r, flecs::world& world)
	: r(r), window(window), next_id(1)
{
	int x, y;
	glfwGetWindowSize(window, &x, &y);
	window_size = vec2(x, y);

	init_callbacks(window);
	init_ecs(world);

	r->imgui()->add_window("Input", [&](bool* open) {
		this->build_gui(open);
	});
}

void input_distributor::init_callbacks(GLFWwindow* window) {
	glfwSetWindowUserPointer(window, this);

	glfwSetWindowSizeCallback(window, [](GLFWwindow* wnd, int w, int h) {
        auto* d = (input_distributor*)glfwGetWindowUserPointer(wnd);
		d->window_size = vec2(w, h);
        d->r->resize(wnd);
    });

	glfwSetCursorPosCallback(window, [](GLFWwindow* wnd, double xpos, double ypos) {
        auto* d = (input_distributor*)glfwGetWindowUserPointer(wnd);
		auto& io = ImGui::GetIO();
		if (!io.WantCaptureMouse) {
			d->process_mouse_pos(vec2(xpos, ypos));
		}
	});
	glfwSetMouseButtonCallback(window, [](GLFWwindow* wnd, int button, int action, int mods) {
        auto* d = (input_distributor*)glfwGetWindowUserPointer(wnd);
        auto& io = ImGui::GetIO();
		if (!io.WantCaptureMouse) {
			d->process_mouse_button(button, action);
		}
	});
	glfwSetKeyCallback(window, [](GLFWwindow* wnd, int key, int scancode, int action, int mods) {
        auto* d = (input_distributor*)glfwGetWindowUserPointer(wnd);
        auto& io = ImGui::GetIO();
		if (!io.WantCaptureKeyboard) {
			d->process_key(key, action);
		}
	});

	ImGui_ImplGlfw_InstallCallbacks(window);
}

void input_distributor::init_ecs(flecs::world& world) {
	world.system<comp::interactable>("Interaction")
		.each([&](flecs::entity e, comp::interactable& intr) {
			intr.model->process_input(input_state, e);
		});
}

void input_distributor::process_mouse_pos(vec2 p) {
	for(const auto&[id, map] : axises) {
		if (map.mouse.has_value()) {
			auto axis = map.mouse.value();
			input_state.axis_states[id] =
				glm::clamp((p[axis] / window_size[axis])*2.0f-1.0f, -1.0f, 1.0f);
		}
	}
}

void input_distributor::process_mouse_button(int button, int action) {
	for (const auto& [id, map] : buttons) {
		if (map.mouse.has_value() && button == map.mouse.value()) {
			auto& s = input_state.button_states[id];
			s = action == GLFW_PRESS;
		}
	}
}

void input_distributor::process_key(int key, int action) {
	if (action == GLFW_RELEASE) {
		if (key == GLFW_KEY_F3) {
			auto current_mode = glfwGetInputMode(window, GLFW_CURSOR);
			int new_mode = -1;
			switch (current_mode) {
			case GLFW_CURSOR_NORMAL: new_mode = GLFW_CURSOR_DISABLED; break;
			case GLFW_CURSOR_DISABLED: new_mode = GLFW_CURSOR_NORMAL; break;
			default: assert(false);
			}
			glfwSetInputMode(window, GLFW_CURSOR, new_mode);
		}
	}
	for (const auto& [id, map] : buttons) {
		if (map.key.has_value() && key == map.key.value()) {
			auto& s = input_state.button_states[id];
			s = action == GLFW_PRESS;
		}
	}
	float axis_value = action == GLFW_PRESS ? 1.0f : 0.0f;
	for (const auto& [id, map] : axises) {
		if (map.key.has_value()) {
			const auto& m = map.key.value();
			if(key == m.negative_key)
				input_state.axis_states[id] =
					-axis_value;
			else if(key == m.positive_key)
				input_state.axis_states[id] =
					axis_value;
		}
	}
}

map_id input_distributor::register_axis_mapping(const char* name, axis_mapping mapping) {
	auto id = next_id;
	next_id++;
	id_names[name] = id;
	axises[id] = mapping;
	return id;
}

map_id input_distributor::register_button_mapping(const char* name, button_mapping mapping) {
	auto id = next_id;
	next_id++;
	id_names[name] = id;
	buttons[id] = mapping;
	return id;
}

map_id input_distributor::id_for_name(const char* name) {
	return id_names[name];
}

void input_distributor::build_gui(bool* open) {
	ImGui::Begin("Input", open);
	if (ImGui::BeginTable("#InputStateTable", 3, ImGuiTableFlags_Resizable)) {
		ImGui::TableSetupColumn("ID");
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("State");
		ImGui::TableHeadersRow();
		for (const auto& [name, id] : id_names) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%u", id);
			ImGui::TableNextColumn();
			ImGui::Text("%s", name);
			ImGui::TableNextColumn();
			auto axis = input_state.axis_states.find(id);
			if (axis != input_state.axis_states.end()) {
				ImGui::Text("%f", axis->second);
			} else {
				auto button = input_state.button_states.find(id);
				if (button != input_state.button_states.end()) {
					ImGui::Text("%s", button->second ? "pressed" : "released");
				} else {
					ImGui::Text("ERROR");
				}
			}
		}
		ImGui::EndTable();
	}
	ImGui::End();
}
