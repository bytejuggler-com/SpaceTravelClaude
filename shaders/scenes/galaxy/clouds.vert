#version 460 core

#include "common/math.glsl"
#include "common/noise.glsl"
#include "scenes/galaxy/galaxy.glsl"

// Camera-facing soft quads (6 vertices each) on the same orbits as the stars.
// Mode 0: glowing gas (pink emission nebulae, some blue), added.
// Mode 1: dust, a thin layer trailing slightly behind the arms, multiplied in to carve dark lanes.

uniform mat4 uViewProj;
uniform vec3 uCamPos;
uniform vec3 uCamRight;
uniform vec3 uCamUp;
uniform int uMode;
uniform float uStrength;

out vec2 vLocal;
out vec3 vColor;

void main() {
    uint id = uint(gl_VertexID / 6) + (uMode == 0 ? 7000000u : 9000000u);
    vec2 corner = quadCorner(gl_VertexID % 6);

    float a = mix(uMode == 0 ? 15.0 : 10.0, 95.0, hashU(id, 2u));
    float thickness = uMode == 0 ? 0.9 : 0.45;
    float phaseOffset = uMode == 1 ? -0.12 : 0.0;
    vec3 center = orbitPosition(a, hashU(id, 5u) * TAU + phaseOffset, gaussian(hashU(id, 3u), hashU(id, 4u)) * thickness);
    float size = uMode == 0 ? mix(2.5, 6.0, hashU(id, 6u)) : mix(3.0, 8.0, hashU(id, 6u));

    vec3 position = center + (uCamRight * corner.x + uCamUp * corner.y) * size;
    gl_Position = uViewProj * vec4(position, 1.0);
    vLocal = corner;

    float nearFade = smoothstep(2.0, 12.0, length(center - uCamPos));  // no screen-filling quads up close
    if (uMode == 0)
        vColor = (hashU(id, 7u) < 0.7 ? vec3(1.0, 0.32, 0.55) : vec3(0.35, 0.55, 1.0)) * 0.06 * uStrength * nearFade;
    else
        vColor = vec3(0.10, 0.13, 0.18) * uStrength * nearFade;  // absorbs blue more than red
}
