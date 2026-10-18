#version 460 core

#include "common/color.glsl"
#include "common/noise.glsl"

// Final pass: chromatic aberration, bloom, exposure, ACES tonemap, vignette, grain, sRGB encode.

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform vec2 uResolution;
uniform float uTime;
uniform float uExposure;
uniform float uBloomIntensity;
uniform float uChromatic;
uniform float uVignette;
uniform float uGrain;

void main() {
    vec2 fromCenter = vUV - 0.5;

    // Radial chromatic aberration: zero at the center, growing toward the corners.
    vec2 shift = fromCenter * dot(fromCenter, fromCenter) * uChromatic * 0.03;
    vec3 col;
    col.r = texture(uScene, vUV - shift).r;
    col.g = texture(uScene, vUV).g;
    col.b = texture(uScene, vUV + shift).b;

    col += texture(uBloom, vUV).rgb * uBloomIntensity;
    col = acesFitted(col * uExposure);

    float r = length(fromCenter) * 1.41421;  // 1.0 at the corners
    col *= 1.0 - uVignette * r * r;

    col = linearToSrgb(col);
    col += (hash12(gl_FragCoord.xy + fract(uTime * 7.31) * 1000.0) - 0.5) * uGrain;
    fragColor = vec4(col, 1.0);
}
