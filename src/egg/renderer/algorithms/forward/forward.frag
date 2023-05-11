#version 450core
#include "scene_data.h"

layout(location = 0) in VertexOutput {
    vec2 tex_coord;
    mat3 normal_to_world;
} finput;

layout(location = 0) out vec4 final_color;

void main() {
    if(uint(object.base_color) == 65535) {
        final_color = vec4(1.0, 0.0, 1.0, 1.0);
        return;
    }

    vec3 normN = normalize(texture(textures[uint(object.normals)], finput.tex_coord).xyz * 2.0 - 1.0);
    vec3 normW = finput.normal_to_world * normN;

    vec4 base_color = texture(textures[uint(object.base_color)], finput.tex_coord);

    final_color = vec4(base_color.xyz * max(0.0, dot(normW, vec3(0.0, 1.0, 0.0))) + vec3(0.07), base_color.w);
}
