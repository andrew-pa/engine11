#version 450core
#include "vertex.h"
#include "scene_data.h"

layout(location = 0) out VertexOutput {
    vec2 tex_coord;
    mat3 normal_to_world;
} voutput;

void main() {
    voutput.tex_coord = tex_coord;
    vec4 positionW = object_model_to_world() * vec4(positionM, 1.0);

    vec3 tW = normalize(vec3(object_model_to_world() * vec4(tangentM, 0.0)));
    vec3 nW = normalize(vec3(object_model_to_world() * vec4(normalM, 0.0)));
    vec3 bW = normalize(cross(nW, tW));
    voutput.normal_to_world = mat3(tW, bW, nW);

    vec4 positionV = camera_view() * positionW;
    gl_Position = camera_proj() * positionV;
}
