#version 450core
#include "scene_data.h"

layout(location = 0) in VertexOutput {
    vec3 view_dir;
} finput;

layout(location = 0) out vec4 final_color;

void main() {
    vec3 sky = texture(env_skybox, finput.view_dir).xyz;
    sky = pow(sky, vec3(1.0 / 2.2));
    final_color = vec4(sky, 1.0);
}
