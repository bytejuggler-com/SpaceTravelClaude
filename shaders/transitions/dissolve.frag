#version 460 core

#include "common/noise.glsl"
#include "common/transition.glsl"

// Chunky pixel dissolve: both images pixelate toward the middle while random blocks flip to the new scene.

void main() {
    float cellPx = floor(mix(1.0, 36.0, sin(3.14159 * uProgress)));
    vec2 cell = floor(gl_FragCoord.xy / cellPx);
    vec2 uv = (cell + 0.5) * cellPx / uResolution;

    float threshold = hash12(floor(gl_FragCoord.xy / 24.0));  // fixed coarse grid so blocks don't flicker
    vec3 col = threshold < uProgress ? toColor(uv) : fromColor(uv);
    fragColor = vec4(col, 1.0);
}
