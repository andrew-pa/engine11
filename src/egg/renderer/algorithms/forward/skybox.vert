#version 450core
#include "scene_data.h"

layout(location = 0) in vec3 positionW;

layout(location = 0) out VertexOutput {
    vec3 view_dir;
} voutput;

void main() {
    vec4 p = camera_proj() * camera_view() * vec4(positionW, 0.0); // use w=0 to prevent translation
    // make sure skybox has NDC depth = 1.0
    gl_Position = p.xyww;
}
