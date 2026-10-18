#version 460 core

#include "common/transition.glsl"

// Slanted soft wipe with a glowing leading edge.

void main() {
    vec2 dir = normalize(vec2(1.0, 0.35));
    float x = dot(vUV, dir) / dot(vec2(1.0), dir);  // 0..1 across the screen along dir
    float edge = mix(-0.08, 1.08, uProgress);

    float showFrom = smoothstep(edge - 0.02, edge + 0.02, x);
    vec3 col = mix(toColor(vUV), fromColor(vUV), showFrom);
    col += vec3(0.3, 0.8, 1.0) * exp(-abs(x - edge) * 90.0) * 5.0;
    fragColor = vec4(col, 1.0);
}
