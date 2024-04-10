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

layout(set = 0, binding = 2) uniform per_frame { vec3 camera_position; };

// TODO: should the set index be configurable?
layout(set = 0, binding = 0) buffer transforms_buffer { mat4 transforms[]; };

layout(set = 0, binding = 1) uniform sampler2D textures[];

layout(set = 0, binding = 4) uniform samplerCube env_skybox;

mat4 camera_view() { return transforms[camera_view_transform_index]; }

mat4 camera_proj() { return transforms[camera_proj_transform_index]; }

mat4 object_model_to_world() { return transforms[object.transform_index]; }

struct light_info {
    vec3 emmitance;
    uint type;
    vec3 position;
    // for point lights: attenuation
    // for spot lights: inner cutoff
    float param1;
    vec3  direction;
    // for spot lights: outer cutoff
    float param2;
};

layout(set = 0, binding = 3) buffer lights_buffer {
    uint       max_index, min_index;
    uint       padding[2];
    light_info info[];
}

lights;

bool compute_light_radiance(in uint i, in vec3 posW, out vec3 L, out vec3 radiance) {
    uint type = lights.info[i].type;
    if(type == 1) {  // directional light
        L        = lights.info[i].direction;
        radiance = lights.info[i].emmitance;
    } else if(type == 2) {  // point light
        vec3  v  = lights.info[i].position - posW;
        float l  = length(v);
        L        = v / l;
        radiance = lights.info[i].emmitance / (1.0 + lights.info[i].param1 * l * l);
    } else if(type == 3) {  // spot light
        L           = normalize(lights.info[i].position - posW);
        float theta = dot(L, lights.info[i].direction);
        float eps   = lights.info[i].param1 - lights.info[i].param2;
        float att   = clamp((theta - lights.info[i].param2) / eps, 0.0, 1.0);
        radiance    = lights.info[i].emmitance * att;
    } else
        return false;
    return true;
}
