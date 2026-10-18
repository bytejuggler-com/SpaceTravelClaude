#version 460 core

#include "common/noise.glsl"

// Deep-space backdrop for the horizontal starfield: dark gradient with faint nebula wisps drifting slowly.

in vec2 vUV;
out vec4 fragColor;

uniform vec2 uResolution;
uniform float uTravel;  // screen widths travelled by the nearest star layer

void main() {
    vec2 p = (gl_FragCoord.xy - 0.5 * uResolution) / uResolution.y;
    vec3 col = mix(vec3(0.003, 0.0, 0.012), vec3(0.0, 0.004, 0.014), p.y + 0.5);
    col += vec3(0.05, 0.012, 0.08) * smoothstep(0.45, 0.85, fbm(p * 1.6 + vec2(uTravel * 0.15, 0.0)));
    col += vec3(0.0, 0.025, 0.06) * smoothstep(0.5, 0.9, fbm(p * 3.0 + vec2(uTravel * 0.25, 7.0)));
    fragColor = vec4(col, 1.0);
}
