#version 460 core

#include "common/color.glsl"

// 13-tap downsample (Jimenez, "Next Generation Post Processing in Call of Duty: Advanced Warfare").
// The first pass also applies the soft threshold and a Karis average to stop single bright pixels flickering.

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uSource;
uniform vec2 uSourceTexel;  // 1 / source size
uniform int uPrefilter;
uniform vec4 uThreshold;    // x: threshold, y: threshold - knee, z: 2 * knee, w: 0.25 / knee

vec3 prefilter(vec3 c) {
    c = min(c, vec3(64.0));  // tame inf/huge values before they smear across the screen
    float brightness = max(c.r, max(c.g, c.b));
    float soft = clamp(brightness - uThreshold.y, 0.0, uThreshold.z);
    soft = soft * soft * uThreshold.w;
    float contribution = max(soft, brightness - uThreshold.x) / max(brightness, 1e-5);
    return c * contribution;
}

float karisWeight(vec3 c) { return 1.0 / (1.0 + luminance(c)); }

vec3 tap(vec2 offset) { return texture(uSource, vUV + uSourceTexel * offset).rgb; }

void main() {
    vec3 a = tap(vec2(-2, 2)), b = tap(vec2(0, 2)), c = tap(vec2(2, 2));
    vec3 d = tap(vec2(-2, 0)), e = tap(vec2(0, 0)), f = tap(vec2(2, 0));
    vec3 g = tap(vec2(-2, -2)), h = tap(vec2(0, -2)), i = tap(vec2(2, -2));
    vec3 j = tap(vec2(-1, 1)), k = tap(vec2(1, 1));
    vec3 l = tap(vec2(-1, -1)), m = tap(vec2(1, -1));

    vec3 result;
    if (uPrefilter == 1) {
        vec3 g0 = prefilter((a + b + d + e) * 0.25);
        vec3 g1 = prefilter((b + c + e + f) * 0.25);
        vec3 g2 = prefilter((d + e + g + h) * 0.25);
        vec3 g3 = prefilter((e + f + h + i) * 0.25);
        vec3 g4 = prefilter((j + k + l + m) * 0.25);
        float w0 = karisWeight(g0) * 0.125, w1 = karisWeight(g1) * 0.125;
        float w2 = karisWeight(g2) * 0.125, w3 = karisWeight(g3) * 0.125;
        float w4 = karisWeight(g4) * 0.5;
        result = (g0 * w0 + g1 * w1 + g2 * w2 + g3 * w3 + g4 * w4) / (w0 + w1 + w2 + w3 + w4);
    } else {
        result = e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125;
    }
    fragColor = vec4(max(result, 0.0), 1.0);
}
