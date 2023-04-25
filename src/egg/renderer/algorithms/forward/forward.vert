#version 450core
#include "vertex.h"
#include "scene_data.h"

layout(location = 0) out VertexOutput {
    vec2 tex_coord;
} voutput;

void main() {
    voutput.tex_coord = tex_coord;
    vec4 positionW = object_model_to_world() * vec4(positionM, 1.0);
    vec4 positionV = camera_view() * positionW;
    gl_Position = camera_proj() * positionV;
}
