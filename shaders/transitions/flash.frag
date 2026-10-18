#version 460 core

#include "common/transition.glsl"

// HDR white flash peaking exactly at the cut, so bloom flares the whole screen and hides the switch.

void main() {
    float flash = exp(-pow((uProgress - 0.5) * 5.0, 2.0));
    vec3 base = uProgress < 0.5 ? fromColor(vUV) : toColor(vUV);
    vec3 col = base + vec3(1.0, 0.95, 0.9) * flash * 6.0;
    fragColor = vec4(col, 1.0);
}
