#version 460 core

#include "common/math.glsl"
#include "common/noise.glsl"
#include "scenes/galaxy/galaxy.glsl"

// One star per vertex (GL_POINTS): warm puffy bulge, thin disc with blue young stars, a few very bright ones.

uniform mat4 uViewProj;
uniform vec3 uCamPos;
uniform float uPointScale;   // pixel size of a star at distance 1 (scaled by resolution on the CPU)
uniform float uBrightness;
uniform float uBulgeFraction;

out vec3 vColor;

void main() {
    uint id = uint(gl_VertexID);
    bool bulge = hashU(id, 1u) < uBulgeFraction;
    float u = hashU(id, 2u);
    float a = bulge ? kBulgeRadius * pow(u, 1.6) : diskRadius(u);
    float thickness = bulge ? 1.0 + 7.0 * (1.0 - a / (kBulgeRadius + 2.0)) : mix(2.5, 0.8, smoothstep(10.0, 60.0, a));
    vec3 position = orbitPosition(a, hashU(id, 5u) * TAU, gaussian(hashU(id, 3u), hashU(id, 4u)) * thickness);

    float temperature = hashU(id, 6u);
    vec3 color = bulge ? mix(vec3(1.0, 0.72, 0.42), vec3(1.0, 0.9, 0.75), temperature)
                       : (temperature < 0.25 ? vec3(0.6, 0.75, 1.0)
                                             : (temperature < 0.7 ? vec3(1.0, 0.95, 0.85) : vec3(1.0, 0.7, 0.45)));
    float luminosity = 0.3 + pow(hashU(id, 7u), 8.0) * 25.0;  // mostly faint, a few giants

    float range = length(position - uCamPos);
    float size = clamp(uPointScale / range * (0.7 + 0.15 * luminosity), 1.0, 6.0);
    gl_PointSize = size;
    gl_Position = uViewProj * vec4(position, 1.0);
    vColor = color * luminosity * uBrightness / size;  // keep total light per star independent of its size
}
