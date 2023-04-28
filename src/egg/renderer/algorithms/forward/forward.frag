#version 450core
#include "scene_data.h"

layout(location = 0) in VertexOutput {
    vec2 tex_coord;
} finput;

layout(location = 0) out vec4 final_color;

void main() {
    if(uint(object.base_color) == 65535) {
        final_color = vec4(1.0, 0.0, 1.0, 1.0);
        return;
    }

    final_color = texture(textures[uint(object.base_color)], finput.tex_coord);
}
