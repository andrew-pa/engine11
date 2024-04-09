
layout(binding = 0) uniform sampler2D input_map;
layout(binding = 1, OUTPUT_MAP_FORMAT) uniform writeonly imageCube output_map;

vec3 compute_cart_coords_from_texel_index(uvec3 i) {
    // assume we run 1 shader per texel.
    vec2 s = (vec2(i.xy) / vec2(imageSize(output_map))) * 2.0 - 1.0;
    vec2 n = vec2(-1.0, 1.0);
    vec3 d;
    switch(i.z) {
        case 0: d = vec3(+1.0, -s.y, -s.x); break;
        case 1: d = vec3(-1.0, -s.y, +s.x); break;
        case 2: d = vec3(+s.x, +1.0, +s.y); break;
        case 3: d = vec3(+s.x, -1.0, -s.y); break;
        case 4: d = vec3(+s.x, -s.y, +1.0); break;
        case 5: d = vec3(-s.x, -s.y, -1.0); break;
    }
    return normalize(d);
}

vec2 compute_spherical_coords_from_texel_index(uvec3 i) {
    vec3 v  = compute_cart_coords_from_texel_index(i);
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= vec2(0.1591, 0.3183);  // (1 / 2π, 1 / π)
    uv += 0.5;
    return uv;
}

#define PI 3.14159265359
