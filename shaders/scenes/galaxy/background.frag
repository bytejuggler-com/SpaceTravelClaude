#version 460 core

#include "common/camera.glsl"
#include "common/noise.glsl"
#include "common/sky.glsl"

// Behind the galaxy particles: distant stars, the unresolved glow of the disc (sampled where the ray crosses
// the disc plane) and the bright, flattened core.

in vec2 vUV;
out vec4 fragColor;

uniform vec2 uResolution;
uniform float uCoreGlow;
uniform float uHaze;

void main() {
    vec3 ro = uCamPos;
    vec3 rd = cameraRay(gl_FragCoord.xy, uResolution);
    vec3 col = starSky(rd, 1.0 / (uResolution.y * uFocal)) * 0.35 + vec3(0.001, 0.001, 0.002);

    // Disc haze where the ray crosses y = 0.
    if (abs(rd.y) > 1e-4) {
        float t = -ro.y / rd.y;
        if (t > 0.0) {
            float r = length((ro + rd * t).xz);
            col += vec3(1.0, 0.85, 0.7) * exp(-r / 30.0) * smoothstep(115.0, 70.0, r) * uHaze;
        }
    }

    // Core: glow around the closest approach to the center in a frame squashed vertically (flattened bulge).
    vec3 scale = vec3(1.0, 2.5, 1.0);
    vec3 q = ro * scale;
    vec3 dq = normalize(rd * scale);
    float tc = max(-dot(q, dq), 0.0);
    float r = length(q + dq * tc);
    col += vec3(1.0, 0.8, 0.55) * (exp(-r / 5.0) * 2.0 + exp(-r / 16.0) * 0.35) * uCoreGlow;

    fragColor = vec4(col, 1.0);
}
