#version 460 core

// 3x3 tent upsample; additively blended onto the next larger bloom mip.

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uSource;
uniform vec2 uSourceTexel;  // 1 / source size
uniform float uRadius;

vec3 tap(vec2 offset) { return texture(uSource, vUV + uSourceTexel * uRadius * offset).rgb; }

void main() {
    vec3 sum = tap(vec2(0, 0)) * 4.0;
    sum += (tap(vec2(-1, 0)) + tap(vec2(1, 0)) + tap(vec2(0, -1)) + tap(vec2(0, 1))) * 2.0;
    sum += tap(vec2(-1, -1)) + tap(vec2(1, -1)) + tap(vec2(-1, 1)) + tap(vec2(1, 1));
    fragColor = vec4(sum / 16.0, 1.0);
}
