#version 460 core

#include "common/transition.glsl"

void main() {
    float t = smoothstep(0.0, 1.0, uProgress);
    fragColor = vec4(mix(fromColor(vUV), toColor(vUV), t), 1.0);
}
