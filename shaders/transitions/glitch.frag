#version 460 core

#include "common/noise.glsl"
#include "common/transition.glsl"

// Digital glitch: displaced scanline bands, RGB split and random blocks of the next scene breaking through.

vec3 splitSample(sampler2D image, vec2 uv, float amount) {
    return vec3(texture(image, uv + vec2(amount, 0.0)).r,
                texture(image, uv).g,
                texture(image, uv - vec2(amount, 0.0)).b);
}

void main() {
    float strength = sin(3.14159 * uProgress);
    float frame = floor(uTime * 24.0);

    float band = floor(vUV.y * 32.0);
    float bandNoise = hash12(vec2(band, frame));
    float shift = (bandNoise - 0.5) * 0.25 * strength * step(0.55, bandNoise);
    vec2 uv = vec2(vUV.x + shift, vUV.y);

    float block = hash12(floor(vUV * vec2(10.0, 18.0)) + frame * 0.37);
    float split = 0.015 * strength;
    vec3 col = block < uProgress ? splitSample(uTo, uv, split) : splitSample(uFrom, uv, split);

    col *= 1.0 - 0.3 * strength * step(0.5, fract(gl_FragCoord.y * 0.5));  // scanlines
    fragColor = vec4(col, 1.0);
}
