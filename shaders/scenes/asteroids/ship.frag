#version 460 core

#include "common/math.glsl"
#include "common/noise.glsl"
#include "common/lighting.glsl"

// Fighter: light grey panelled hull with a red stripe, dark glossy canopy, glowing engines, blinking
// navigation lights. Material ids from src/scenes/asteroids/Fighter.h.

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vObjectPos;
in vec4 vColor;
flat in float vMaterial;
out vec4 fragColor;

uniform vec3 uCamPos;
uniform float uTime;
uniform float uAmbient;
uniform vec3 uEngineColor;
uniform float uEngineGlow;

void main() {
    int material = int(vMaterial + 0.5);
    if (material == 2) {  // engine nozzle
        float flicker = 0.9 + 0.1 * sin(uTime * 37.0) * sin(uTime * 23.0);
        fragColor = vec4(uEngineColor * uEngineGlow * flicker, 1.0);
        return;
    }
    if (material >= 3) {  // navigation lights, blinking alternately
        float blink = step(0.75, fract(uTime * 1.2 + (material == 4 ? 0.5 : 0.0)));
        vec3 light = material == 3 ? vec3(1.0, 0.08, 0.04) : vec3(0.1, 1.0, 0.2);
        fragColor = vec4(light * (0.4 + 14.0 * blink), 1.0);
        return;
    }

    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 L = uLightDir;
    vec3 p = vObjectPos;

    bool glass = material == 1;
    float panels = step(0.035, fract(p.z * 3.0)) * step(0.035, fract(p.x * 2.5 + 0.5));
    vec3 albedo = vec3(0.55, 0.57, 0.6) * mix(0.55, 1.0, panels) * mix(0.9, 1.05, valueNoise(p * 9.0));
    if (abs(p.x) < 0.07 && p.y > 0.0 && p.z < 0.6) albedo = vec3(0.7, 0.12, 0.04);  // racing stripe
    if (glass) albedo = vec3(0.015, 0.02, 0.035);

    float shadow = shadowFactor(vWorldPos, N);
    float fresnel = fresnelSchlick(max(dot(N, V), 0.0), glass ? 0.06 : 0.04);
    vec3 col = albedo * uLightColor * max(dot(N, L), 0.0) * shadow;
    col += uLightColor * specular(N, V, L, glass ? 0.08 : 0.35) * fresnel * (glass ? 4.0 : 2.0) * shadow;
    col += albedo * uAmbient * vec3(0.45, 0.55, 0.8) * (0.6 + 0.4 * N.y);
    if (glass) col += vec3(0.25, 0.35, 0.6) * fresnel * 0.4;  // sky reflection on the canopy

    fragColor = vec4(col, 1.0);
}
