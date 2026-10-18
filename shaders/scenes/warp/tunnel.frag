#version 460 core

#include "common/math.glsl"
#include "common/noise.glsl"

// Hyperspace tunnel behind the warp stars: radial energy streaks rushing past and a glowing vanishing point.
// Fades in with uHyper.

in vec2 vUV;
out vec4 fragColor;

uniform vec2 uResolution;
uniform float uTravel;  // world units travelled
uniform float uHyper;   // 0 = normal space, 1 = full hyperspace
uniform float uRoll;    // camera roll, so the streaks turn with the stars

void main() {
    vec2 p = rotate2d(-uRoll) * ((gl_FragCoord.xy - 0.5 * uResolution) / uResolution.y);
    float r = length(p);
    vec2 dir = p / max(r, 1e-4);

    vec3 col = vec3(0.004, 0.002, 0.012) * (1.0 - uHyper);  // faint backdrop before the jump
    if (uHyper > 0.0) {
        // Tunnel coordinate: 1/r maps screen radius to depth; high angular frequency, low depth frequency
        // stretches the noise into radial streaks.
        float depth = 0.25 / max(r, 0.02) + uTravel * 0.04;
        float n = fbm(vec3(dir * 5.0, depth * 0.6));
        float streaks = smoothstep(0.45, 0.85, n);

        vec3 tunnel = mix(vec3(0.1, 0.25, 1.0), vec3(0.75, 0.25, 1.0), n) * streaks * 2.5;
        tunnel *= smoothstep(0.03, 0.4, r);
        tunnel += vec3(0.6, 0.75, 1.0) * exp(-r * 7.0) * 2.0;  // vanishing-point glow
        col += tunnel * uHyper;
    }
    fragColor = vec4(col, 1.0);
}
