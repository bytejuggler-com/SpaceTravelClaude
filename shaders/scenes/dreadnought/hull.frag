#version 460 core

#include "common/math.glsl"
#include "common/noise.glsl"
#include "common/lighting.glsl"

// Dreadnought hull: cold grey metal plating. Panels with seams and per-plate tone/roughness, small raised
// blocks as bump detail, low grazing sun with soft self-shadows, blue bounce light from the planet below,
// a simple environment reflection (dark space above, planet glow below) and blue engine light on the stern.
// Material ids from src/scenes/dreadnought/DreadnoughtModel.h.

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vObjectPos;
in vec4 vColor;
flat in float vMaterial;
in float vOcclusion;  // baked contact occlusion (blocks are darker at their base)
out vec4 fragColor;

uniform vec3 uCamPos;
uniform float uTime;
uniform float uAmbient;
uniform float uBounce;       // planet bounce light
uniform float uReflect;      // environment reflection strength
uniform float uPanelScale;
uniform vec3 uEngineColor;
uniform float uEngineGlow;
uniform vec3 uEnginePos;
uniform float uWindowGlow;

// 2D coordinates in the plane the surface faces most (planar mapping by dominant normal axis).
vec2 planeCoords(vec3 p, vec3 n) {
    vec3 a = abs(n);
    if (a.y >= a.x && a.y >= a.z) return p.xz;
    if (a.x >= a.z) return p.zy;
    return p.xy;
}

// Rectangular plates, some split in two; returns a per-plate random value and the distance to the nearest seam.
// Plate size and orientation change from region to region so the plating never reads as one repeated grid.
vec2 panel(vec2 uv) {
    vec2 region = floor(uv / 28.0);
    float regionHash = hash12(region + 0.37);
    vec2 size = vec2(6.0, 3.0) * uPanelScale * mix(0.55, 1.6, regionHash);
    if (fract(regionHash * 7.1) < 0.4) size = size.yx;  // plates run the other way
    vec2 g = uv / size;
    vec2 cell = floor(g);
    vec2 f = fract(g);
    float split = hash12(cell);
    if (split < 0.35) {
        float side = step(0.5, f.x);
        cell.x += side * 0.5;
        f.x = fract(f.x * 2.0);
        size.x *= 0.5;
    } else if (split < 0.6) {
        float side = step(0.5, f.y);
        cell.y += side * 0.5;
        f.y = fract(f.y * 2.0);
        size.y *= 0.5;
    }
    vec2 d = min(f, 1.0 - f) * size;
    return vec2(hash12(cell * 1.37 + 3.1), min(d.x, d.y));
}

// Small raised blocks scattered on the plating (real geometry arrives in Phase 9).
float blocks(vec2 uv) {
    vec2 cell = floor(uv);
    vec2 f = fract(uv);
    float h = hash12(cell + 71.0);
    if (h < 0.8) return 0.0;
    vec2 extent = vec2(0.2 + 0.25 * hash12(cell + 1.0), 0.2 + 0.25 * hash12(cell + 2.0));
    vec2 d = abs(f - 0.5) - extent;
    return step(max(d.x, d.y), 0.0) * (0.1 + 0.2 * h);
}

void main() {
    int material = int(vMaterial + 0.5);
    if (material == 1) {  // engine discs
        float flicker = 0.95 + 0.05 * sin(uTime * 31.0) * sin(uTime * 17.0);
        fragColor = vec4(uEngineColor * uEngineGlow * flicker, 1.0);
        return;
    }
    if (material == 4) {  // windows: mostly warm, a few cold, some dark
        float h = hash13(floor(vObjectPos * 2.0) + 0.5);
        vec3 tint = h < 0.15 ? vec3(0.6, 0.75, 1.0) : vec3(1.0, 0.82, 0.55);
        float lit = step(0.12, fract(h * 13.7));
        fragColor = vec4(tint * uWindowGlow * lit * mix(0.6, 1.2, fract(h * 7.3)), 1.0);
        return;
    }

    vec3 geometric = normalize(vNormal);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 L = uLightDir;

    vec2 uv = planeCoords(vObjectPos, geometric);
    vec2 plate = panel(uv);
    float seam = smoothstep(0.0, 0.07, plate.y);
    float height = seam * 0.04 + blocks(uv);
    vec3 N = perturbNormal(geometric, vWorldPos, height, 1.0);

    float roughness = mix(0.35, 0.7, plate.x);
    vec3 albedo = vec3(0.3, 0.315, 0.335) * mix(0.7, 1.15, plate.x) * mix(0.35, 1.0, seam);
    // Weathering: broad tonal patches and grime streaks running along the hull.
    albedo *= mix(0.72, 1.08, fbm(vObjectPos * 0.05));
    albedo *= mix(0.8, 1.0, smoothstep(0.35, 0.75, valueNoise(vec3(vObjectPos.x * 1.5, vObjectPos.y * 1.5, vObjectPos.z * 0.08))));
    if (material == 2) albedo *= 0.12;  // dark recesses
    if (material == 3) {                 // domes: smoother, slightly lighter
        albedo = vec3(0.5, 0.52, 0.55);
        roughness = 0.25;
    }

    float shadow = shadowFactor(vWorldPos, geometric);
    float occlusion = vOcclusion;
    float fresnel = fresnelSchlick(max(dot(N, V), 0.0), 0.06);
    vec3 col = albedo * uLightColor * max(dot(N, L), 0.0) * shadow * mix(1.0, occlusion, 0.5);
    col += uLightColor * specular(N, V, L, roughness) * fresnel * 1.8 * shadow * occlusion;

    // Fill: blue bounce from the planet below, faint cold ambient elsewhere (fully occluded in crevices).
    col += albedo * (uBounce * vec3(0.18, 0.3, 0.55) * max(-N.y, 0.0) + uAmbient * vec3(0.5, 0.6, 0.8)) * occlusion;

    // Environment reflection: black space above, the planet's glow below.
    vec3 R = reflect(-V, N);
    vec3 env = vec3(0.003, 0.004, 0.008) + vec3(0.12, 0.25, 0.5) * smoothstep(0.0, -0.4, R.y) * uBounce;
    col += env * fresnel * uReflect * (1.0 - roughness) * 4.0 * occlusion;

    // Engine glow washing the stern.
    vec3 toEngine = uEnginePos - vWorldPos;
    float engineFalloff = 1.0 / (1.0 + dot(toEngine, toEngine) * 0.01);
    col += albedo * uEngineColor * uEngineGlow * 0.5 * max(dot(N, normalize(toEngine)), 0.0) * engineFalloff;

    fragColor = vec4(col, 1.0);
}
