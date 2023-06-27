#version 450core
#include "scene_data.h"
#include "pbr_brdf.h"

layout(location = 0) in VertexOutput {
    vec3 positionW;
    vec2 tex_coord;
    mat3 normal_to_world;
} finput;

layout(location = 0) out vec4 final_color;

void main() {
    if(uint(object.base_color) == 65535) {
        final_color = vec4(0.1, 0.1, 0.1, 1.0);
        return;
    }

    vec3 V = normalize(camera_position - finput.positionW);

    vec4 base_color = texture(textures[uint(object.base_color)], finput.tex_coord);
    base_color.xyz = pow(base_color.xyz, vec3(2.2));
    // TODO: these should really be in the same texture map
    float roughness = texture(textures[uint(object.roughness)], finput.tex_coord).x;
    float metallic  = texture(textures[uint(object.metallic)], finput.tex_coord).x;
    vec3 normN = normalize(texture(textures[uint(object.normals)], finput.tex_coord).xyz * 2.0 - 1.0);

    vec3 N = normalize(finput.normal_to_world * normN);

    vec3 F0 = compute_F0(base_color.xyz, metallic);

    vec3 color = vec3(0.003) * base_color.xyz;
    
    vec3 L = vec3(1.0, 0.0, 0.0), radiance = vec3(100.0, 0.0, 100.0);
    for(uint i = lights.min_index; i < lights.max_index; ++i) {
    //for(uint i = 15; i < 16; ++i) {
        if(!compute_light_radiance(i, finput.positionW, L, radiance)) continue;
		vec3 H = normalize(V+L), kD;
		float NdotH = max(0.0, dot(N, H)),
			  NdotV = max(0.0, dot(N, V)),
			  NdotL = max(0.0, dot(N, L)),
			  HdotV = max(0.0, dot(H, V));

		vec3 specular = cook_torrance_brdf(
			NdotH, NdotV, NdotL, HdotV,
			F0, roughness, metallic, kD
		);

		color += (kD * base_color.xyz / PI + specular) * radiance * NdotL;
        //color += radiance;
    }

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    final_color = vec4(color, 1.0);
}
