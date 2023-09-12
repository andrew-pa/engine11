#version 450core
#include "scene_data.h"

layout(location = 0) in vec3 positionW;

layout(location = 0) out VertexOutput {
    vec3 view_dir;
} voutput;

void main() {
        vec2 vertices[3] = vec2[3](
            vec2(3,1),
            vec2(-1, -3),
            vec2(-1,1)
        );

        gl_Position = vec4(vertices[gl_VertexIndex],1,1);

        vec2 sc = 0.5 * gl_Position.xy + vec2(0.5);
        voutput.view_dir = normalize((camera_view() * vec4(sc * 2.0 - 1.0, 1.0, 0.0)).xyz);
}

// void main() {
//     mat4 v = camera_view();
//     v[3] = vec4(0.0, 0.0, 0.0, 1.0);
//     vec4 p = camera_proj() * camera_view() * vec4(positionW*10.0, 1.0); // use w=0 to prevent translation
//     voutput.view_dir = normalize(positionW * vec3(-1.0, -1.0, 1.0));
//     // make sure skybox has NDC depth = 1.0
//     gl_Position = p.xyww;
// }
