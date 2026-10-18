#version 460 core

#include "common/noise.glsl"

// Outrun neon grid: perspective grid scrolling toward the viewer, striped sun on the horizon. Linear HDR output.

in vec2 vUV;
out vec4 fragColor;

uniform float uTime;
uniform vec2 uResolution;
uniform float uScroll;   // grid travel in cells (one per beat at speed 1)
uniform float uSunSize;
uniform float uGridGlow;
uniform float uKick;     // 1 on each kick, decaying
uniform float uSnare;    // 1 on each snare, decaying

const float kHorizon = -0.08;

vec3 sky(vec2 p) {
    float h = p.y - kHorizon;
    vec3 col = mix(vec3(0.20, 0.01, 0.16), vec3(0.002, 0.0, 0.02), clamp(h * 2.2, 0.0, 1.0));

    // Sparse stars
    vec2 cell = floor(p * 90.0);
    float star = hash12(cell);
    if (star > 0.985) {
        float d = length(fract(p * 90.0) - 0.5);
        col += vec3(0.9, 0.9, 1.0) * smoothstep(0.25, 0.0, d) * (star - 0.985) * 120.0 * smoothstep(0.05, 0.4, h);
    }

    // Striped sun: gaps widen toward the bottom and scroll downward.
    vec2 sunCenter = vec2(0.0, kHorizon + uSunSize * 0.85);
    float d = length(p - sunCenter);
    float below = clamp((sunCenter.y - p.y) / uSunSize, 0.0, 1.0);
    float stripe = step(below * 0.65, fract((p.y - sunCenter.y) * 28.0 / uSunSize * 0.25 + uTime * 0.35));
    float sunMask = smoothstep(uSunSize, uSunSize - 0.004, d) * (p.y > sunCenter.y ? 1.0 : stripe);
    float heightInSun = clamp((p.y - (sunCenter.y - uSunSize)) / (2.0 * uSunSize), 0.0, 1.0);
    vec3 sunColor = mix(vec3(1.0, 0.08, 0.45), vec3(1.0, 0.85, 0.15), heightInSun) * 4.0 * (1.0 + 0.8 * uSnare);
    col = mix(col, sunColor, sunMask);
    col += vec3(1.0, 0.25, 0.45) * exp(-max(d - uSunSize, 0.0) * 9.0) * 0.35;
    return col;
}

vec3 ground(vec2 p) {
    float below = kHorizon - p.y;               // > 0 under the horizon
    float depth = 0.3 / below;                  // perspective distance
    vec2 g = vec2(p.x * depth * 2.2, depth + uScroll);

    vec2 distToLine = 0.5 - abs(fract(g) - 0.5);
    vec2 width = fwidth(g) * 1.2;
    vec2 lines = 1.0 - smoothstep(vec2(0.0), width + 0.02, distToLine);
    float line = max(lines.x, lines.y);

    float fade = smoothstep(0.0, 0.12, below);  // hide aliasing near the horizon
    vec3 col = vec3(0.006, 0.0, 0.02);
    col += vec3(1.0, 0.15, 0.85) * line * fade * uGridGlow * (1.0 + 1.5 * uKick);
    col += vec3(1.0, 0.3, 0.5) * exp(-below * 18.0) * 0.6;  // sun reflection haze
    return col;
}

void main() {
    vec2 p = (gl_FragCoord.xy - 0.5 * uResolution) / uResolution.y;
    vec3 col = p.y > kHorizon ? sky(p) : ground(p);
    col += vec3(1.0, 0.35, 0.8) * exp(-abs(p.y - kHorizon) * 140.0) * 2.0 * (1.0 + uKick);  // horizon glow line
    fragColor = vec4(col, 1.0);
}
