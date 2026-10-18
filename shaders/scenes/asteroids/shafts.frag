#version 460 core

#include "common/noise.glsl"

// Light shafts (screen-space radial blur): march from each pixel toward the star's screen position,
// accumulating star glow wherever the sky is visible (depth = 1). Rocks between interrupt the light and
// carve dark streaks into the rays. Rendered at reduced resolution and added in the composite pass.

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uDepth;
uniform vec2 uResolution;   // of this (reduced) target
uniform vec2 uLightUV;      // star position on screen, may lie outside [0, 1]
uniform float uLightInFront;
uniform float uStrength;
uniform float uDecay;
uniform vec3 uStarColor;

const int kSamples = 48;

void main() {
    if (uLightInFront <= 0.0 || uStrength <= 0.0) {
        fragColor = vec4(0.0);
        return;
    }
    vec2 uv = gl_FragCoord.xy / uResolution;
    float aspect = uResolution.x / uResolution.y;
    vec2 stride = (uLightUV - uv) / float(kSamples);
    vec2 p = uv + stride * hash12(gl_FragCoord.xy);  // jitter hides sampling bands

    float decay = 1.0;
    float sum = 0.0;
    for (int i = 0; i < kSamples; ++i) {
        p += stride;
        bool inside = all(greaterThanEqual(p, vec2(0.0))) && all(lessThanEqual(p, vec2(1.0)));
        float sky = inside && texture(uDepth, p).r >= 1.0 ? 1.0 : 0.0;
        float glow = exp(-length((p - uLightUV) * vec2(aspect, 1.0)) * 5.0);
        sum += sky * glow * decay;
        decay *= uDecay;
    }
    fragColor = vec4(uStarColor * sum / float(kSamples) * uStrength, 1.0);
}
