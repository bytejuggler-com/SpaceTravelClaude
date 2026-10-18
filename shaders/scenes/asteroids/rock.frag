#version 460 core

#include "common/math.glsl"
#include "common/noise.glsl"
#include "common/lighting.glsl"

// Asteroid surface: grey/brown albedo with grit, bumpy detail from object-space noise (sticks to the rotating
// rock), hard key light with soft cascaded shadows, almost no ambient, and a thin rim where the rock is backlit.

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vObjectPos;
in vec4 vColor;       // rgb albedo, a = size (world units)
flat in float vMaterial;
out vec4 fragColor;

uniform vec3 uCamPos;
uniform float uAmbient;
uniform float uBump;
uniform float uRim;

float rockHeight(vec3 p) {
    return valueNoise(p * 3.0) * 0.5 + valueNoise(p * 8.0) * 0.3 + valueNoise(p * 21.0) * 0.2;
}

void main() {
    vec3 geometric = normalize(vNormal);
    float height = rockHeight(vObjectPos * 2.0) * vColor.a;
    vec3 N = perturbNormal(geometric, vWorldPos, height * 0.06, uBump);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 L = uLightDir;

    float grit = valueNoise(vObjectPos * 6.0);
    vec3 albedo = vColor.rgb * mix(0.6, 1.15, grit) * mix(0.75, 1.0, rockHeight(vObjectPos * 0.7));

    float shadow = shadowFactor(vWorldPos, geometric);
    float diffuse = max(dot(N, L), 0.0);
    vec3 col = albedo * uLightColor * diffuse * shadow;
    col += uLightColor * specular(N, V, L, 0.6) * 0.03 * shadow;

    // Cold, faint fill from the rest of the sky so shadows are not pure black.
    col += albedo * uAmbient * vec3(0.45, 0.55, 0.8) * (0.6 + 0.4 * N.y);

    // Backlit rim: bright edge where the star is behind the rock.
    float rim = pow(1.0 - max(dot(N, V), 0.0), 4.0) * max(dot(-V, L), 0.0);
    col += uLightColor * albedo * rim * uRim;

    fragColor = vec4(col, 1.0);
}
