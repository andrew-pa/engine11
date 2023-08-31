#include "egg/input/camera_interaction_models.h"
#include "egg/input/input_distributor.h"
#include "egg/components.h"

fly_camera_interaction_model::fly_camera_interaction_model(float speed)
	: move_x(0), move_y(0), move_z(0), yaw(0), pitch(0), speed(speed)
{

}

void fly_camera_interaction_model::register_with_distributor(class input_distributor* dist) {
	move_x = dist->register_axis_mapping("Move Camera X", axis_mapping{
		.key = key_axis_mapping{GLFW_KEY_A, GLFW_KEY_D}
	});
	move_y = dist->register_axis_mapping("Move Camera Y", axis_mapping{
		.key = key_axis_mapping{GLFW_KEY_Q, GLFW_KEY_E}
	});
	move_z = dist->register_axis_mapping("Move Camera Z", axis_mapping{
		.key = key_axis_mapping{GLFW_KEY_S, GLFW_KEY_W}
	});
	yaw = dist->register_axis_mapping("Rotate Camera Y", axis_mapping{
		.mouse = mouse_axis_mapping{.axis=0, .mode=mouse_mapping_mode::delta}
	});
	pitch = dist->register_axis_mapping("Rotate Camera X", axis_mapping{
		.mouse = mouse_axis_mapping{.axis=1, .mode=mouse_mapping_mode::delta}
	});
}

void fly_camera_interaction_model::process_input(const mapped_input& input, flecs::entity e, float dt) {
	vec3 pos = e.get<comp::position>()->pos;
	quat rot = e.get<comp::rotation>()->rot;

	glm::mat3 basis = glm::toMat3(rot);

	pos += basis * vec3(
		input.axis_states.at(move_x),
		input.axis_states.at(move_y),
		-input.axis_states.at(move_z)
	) * speed * dt;

	vec2 np = vec2(-input.axis_states.at(yaw), input.axis_states.at(pitch)) * glm::pi<float>();
	rot = rot * glm::angleAxis(np.x, vec3(0.f, 1.f, 0.f));
	rot = rot * glm::angleAxis(np.y, vec3(1.f, 0.f, 0.f));
	rot = glm::normalize(rot);

	e.set<comp::position>({ pos });
	e.set<comp::rotation>({ rot });
}
