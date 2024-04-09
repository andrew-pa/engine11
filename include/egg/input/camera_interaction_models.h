#pragma once
#include "egg/input/model.h"

class fly_camera_interaction_model : public interaction_model {
    map_id move_x, move_y, move_z, yaw, pitch;
    float  speed;

  public:
    fly_camera_interaction_model(float speed);
    virtual void register_with_distributor(class input_distributor* dist) override;
    virtual void process_input(const mapped_input& input, flecs::entity e, float dt) override;
    virtual ~fly_camera_interaction_model() override = default;
};
