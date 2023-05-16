#version 450core
#include "vertex.h"
#include "scene_data.h"

layout(location = 0) out VertexOutput {
    vec3 positionW;
    vec2 tex_coord;
    mat3 normal_to_world;
} voutput;

void main() {
    voutput.tex_coord = tex_coord;
    mat4 MtoW = object_model_to_world();

    vec3 tW = normalize(vec3(MtoW * vec4(tangentM, 0.0)));
    vec3 nW = normalize(vec3(MtoW * vec4(normalM, 0.0)));
    vec3 bW = normalize(cross(nW, tW));
    voutput.normal_to_world = mat3(tW, bW, nW);

    vec4 posW =  MtoW * vec4(positionM, 1.0);
    voutput.positionW = posW.xyz;

    gl_Position = camera_proj() * camera_view() * posW;
}
