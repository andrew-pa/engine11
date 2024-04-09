
#define PI 3.1415926535

vec3 compute_F0(vec3 base_color, float metallic) { return mix(vec3(0.04), base_color, metallic); }

vec3 fresnel_Schlick(float HdotV, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - HdotV, 0.0, 1.0), 5.0);
}

float distribution_GGX(float NdotH, float roughness) {
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH2 = NdotH * NdotH;

    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom       = PI * denom * denom;

    return num / denom;
}

float geometry_SchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float geometry_Smith(float NdotV, float NdotL, float roughness) {
    float ggx2 = geometry_SchlickGGX(NdotV, roughness);
    float ggx1 = geometry_SchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 cook_torrance_brdf(
    float    NdotH,
    float    NdotV,
    float    NdotL,
    float    HdotV,
    vec3     F0,
    float    roughness,
    float    metallic,
    out vec3 kD
) {
    float D = distribution_GGX(NdotH, roughness);
    float G = geometry_Smith(NdotV, NdotL, roughness);
    vec3  F = fresnel_Schlick(HdotV, F0);

    vec3 kS = F;
    kD      = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float d = 4.0 * NdotV * NdotL + 0.0001;

    return (D * G * F) / d;
}