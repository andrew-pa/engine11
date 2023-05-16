#extension GL_EXT_shader_16bit_storage : enable
#extension GL_EXT_nonuniform_qualifier : enable

struct per_object_push_constants {
    uint     transform_index;
    uint16_t base_color, normals, roughness, metallic;
};

layout(push_constant) uniform per_object_pc {
    uint                      camera_view_transform_index, camera_proj_transform_index;
    per_object_push_constants object;
};

layout(set = 0, binding = 2) uniform per_frame {
    vec3 camera_position;
};

// TODO: should the set index be configurable?
layout(set = 0, binding = 0) buffer transforms_buffer { mat4 transforms[]; };

layout(set = 0, binding = 1) uniform sampler2D textures[];

mat4 camera_view() { return transforms[camera_view_transform_index]; }

mat4 camera_proj() { return transforms[camera_proj_transform_index]; }

mat4 object_model_to_world() { return transforms[object.transform_index]; }
