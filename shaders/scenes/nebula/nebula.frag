#version 460 core

#include "common/noise.glsl"

// Drifting domain-warped nebula over parallax star layers. Outputs linear HDR (bright cores feed the bloom).

in vec2 vUV;
out vec4 fragColor;

uniform float uTime;
uniform vec2 uResolution;
uniform float uWarp;
uniform float uStarBrightness;
uniform float uGlow;
uniform float uBeatPulse;  // 1 on each beat, decaying

vec3 starLayer(vec2 p, float scale, float speed, float seed) {
    vec2 sp = p * scale + vec2(uTime * speed, 0.0);
    vec2 cell = floor(sp);
    vec2 local = fract(sp) - 0.5;
    float h = hash12(cell + seed);
    if (h < 0.9) return vec3(0.0);

    vec2 offset = (hash22(cell + seed) - 0.5) * 0.7;
    float d = length(local - offset);
    float twinkle = 0.6 + 0.4 * sin(uTime * 3.0 + h * 60.0);
    vec3 tint = mix(vec3(0.7, 0.8, 1.0), vec3(1.0, 0.85, 0.7), fract(h * 13.0));
    return tint * twinkle * (smoothstep(0.12, 0.0, d) + 0.15 * smoothstep(0.45, 0.0, d));
}

void main() {
    vec2 p = (gl_FragCoord.xy - 0.5 * uResolution) / uResolution.y;
    float t = uTime * 0.04;

    vec2 q = vec2(fbm(p * 2.0 + vec2(t, 0.0)), fbm(p * 2.0 + vec2(5.2, -t)));
    vec2 r = vec2(fbm(p * 2.5 + uWarp * q + vec2(1.7, 9.2) + t), fbm(p * 2.5 + uWarp * q + vec2(8.3, 2.8) - t));
    float n = fbm(p * 2.0 + uWarp * r);

    vec3 col = vec3(0.004, 0.0, 0.012);
    col = mix(col, vec3(0.18, 0.01, 0.15), smoothstep(0.35, 0.75, n));
    col = mix(col, vec3(0.01, 0.12, 0.45), smoothstep(0.4, 0.9, length(q) * n));
    col += vec3(1.0, 0.45, 0.15) * pow(smoothstep(0.5, 0.95, n * r.x * 1.6), 3.0) * uGlow * (1.0 + 1.5 * uBeatPulse);

    col += starLayer(p, 25.0, 0.4, 0.0) * 0.4 * uStarBrightness;
    col += starLayer(p, 45.0, 0.8, 17.0) * 0.7 * uStarBrightness;
    col += starLayer(p, 80.0, 1.6, 31.0) * uStarBrightness;

    fragColor = vec4(col, 1.0);
}
