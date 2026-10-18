#version 460 core

#include "common/noise.glsl"

// Banded gas giant drifting past behind the near star layers. Premultiplied alpha output
// (blend ONE, ONE_MINUS_SRC_ALPHA): the disc covers what is behind it, the atmosphere halo adds light.

in vec2 vUV;
out vec4 fragColor;

uniform vec2 uResolution;
uniform vec2 uCenter;    // in screen-height units, origin at the screen center
uniform float uRadius;
uniform float uRotation;

void main() {
    vec2 p = (gl_FragCoord.xy - 0.5 * uResolution) / uResolution.y;
    vec2 d = (p - uCenter) / uRadius;
    float r = length(d);
    if (r > 1.8) {
        fragColor = vec4(0.0);
        return;
    }

    vec3 lightDir = normalize(vec3(-0.65, 0.4, 0.6));
    float pixel = 1.0 / (uResolution.y * uRadius);
    float coverage = smoothstep(1.0 + pixel, 1.0 - pixel, r);

    vec3 col = vec3(0.0);
    if (coverage > 0.0) {
        vec3 n = vec3(d, sqrt(max(0.0, 1.0 - r * r)));

        // Rotate around the (slightly tilted) vertical axis for the surface pattern.
        float c = cos(uRotation), s = sin(uRotation);
        vec3 q = vec3(c * n.x + s * n.z, n.y, -s * n.x + c * n.z);
        float latitude = asin(clamp(n.y + 0.15 * n.x, -1.0, 1.0));
        float turbulence = fbm(q * 3.0);
        float bands = sin(latitude * 14.0 + turbulence * 4.0);

        vec3 base = mix(vec3(0.5, 0.3, 0.16), vec3(0.95, 0.8, 0.6), 0.5 + 0.5 * bands);
        base = mix(base, vec3(0.8, 0.35, 0.18), smoothstep(0.62, 0.85, fbm(q * 6.0 + 3.0)));  // storms

        float diffuse = max(dot(n, lightDir), 0.0);
        col = base * (0.015 + 1.5 * diffuse) * mix(0.55, 1.0, n.z);                          // limb darkening
        col += vec3(0.35, 0.55, 1.0) * pow(1.0 - n.z, 3.0) * smoothstep(-0.1, 0.4, dot(n, lightDir)) * 0.8;
    }

    // Thin atmosphere glow around the lit limb.
    float halo = exp(-max(r - 1.0, 0.0) * 14.0) * (1.0 - coverage);
    float lit = 0.25 + 0.75 * max(dot(normalize(d), normalize(lightDir.xy)), 0.0);
    vec3 glow = vec3(0.35, 0.55, 1.0) * halo * lit * 0.7;

    fragColor = vec4(col * coverage + glow, coverage);
}
