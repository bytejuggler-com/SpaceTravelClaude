#version 460 core

#include "common/math.glsl"
#include "common/noise.glsl"
#include "common/lighting.glsl"

// Vector balls: glossy tinted spheres with a sharp key highlight, a magenta rim light and a reflection of a
// simple studio environment (gradient plus two bright panels).

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vObjectPos;
in vec4 vColor;
flat in float vMaterial;
out vec4 fragColor;

uniform vec3 uCamPos;

vec3 environment(vec3 r) {
    vec3 col = mix(vec3(0.03, 0.01, 0.06), vec3(0.2, 0.25, 0.5), smoothstep(-0.3, 0.8, r.y));
    col += vec3(2.0) * smoothstep(0.92, 0.97, dot(r, normalize(vec3(-0.6, 0.6, 0.5))));  // softbox
    col += vec3(1.2, 0.4, 1.0) * smoothstep(0.9, 0.96, dot(r, normalize(vec3(0.8, 0.1, -0.3))));
    return col;
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 key = normalize(vec3(-0.6, 0.7, 0.6));
    vec3 rim = normalize(vec3(0.9, 0.2, -0.4));
    float fresnel = fresnelSchlick(max(dot(N, V), 0.0), 0.08);

    vec3 col = vColor.rgb * (0.08 + 0.9 * max(dot(N, key), 0.0));
    col += vec3(1.0) * specular(N, V, key, 0.15) * 0.9;
    col += vec3(1.0, 0.35, 0.9) * pow(max(dot(N, rim), 0.0), 3.0) * 0.6;
    col += environment(reflect(-V, N)) * mix(0.25, 1.0, fresnel);
    fragColor = vec4(col, 1.0);
}
