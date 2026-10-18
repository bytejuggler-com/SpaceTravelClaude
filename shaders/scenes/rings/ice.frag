#version 460 core

#include "common/math.glsl"
#include "common/noise.glsl"
#include "common/lighting.glsl"
#include "scenes/rings/rings.glsl"

// Icy ring chunks near the camera: bright frosty albedo with grit, sharp sun highlight, the planet's shadow
// (analytic), a cold fill from the planet's lit face, and a fade with distance into the ring sheet.

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vObjectPos;
in vec4 vColor;  // rgb tint, a = size
flat in float vMaterial;
out vec4 fragColor;

uniform vec3 uCamPos;
uniform vec3 uSunColor;

void main() {
    vec3 geometric = normalize(vNormal);
    float height = (valueNoise(vObjectPos * 6.0) * 0.6 + valueNoise(vObjectPos * 15.0) * 0.4) * vColor.a * 0.05;
    vec3 N = perturbNormal(geometric, vWorldPos, height, 1.0);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 L = uLightDir;

    vec3 albedo = vColor.rgb * mix(0.75, 1.1, valueNoise(vObjectPos * 4.0));
    float sun = planetShadow(vWorldPos, L);
    vec3 col = albedo * uSunColor * max(dot(N, L), 0.0) * sun;
    col += uSunColor * specular(N, V, L, 0.25) * fresnelSchlick(max(dot(N, V), 0.0), 0.05) * 2.0 * sun;
    col += albedo * vec3(0.5, 0.45, 0.38) * 0.06 * max(dot(N, -normalize(vWorldPos)), 0.0);  // light from the planet
    col += albedo * vec3(0.4, 0.45, 0.6) * 0.015;
    fragColor = vec4(col, 1.0);
}
