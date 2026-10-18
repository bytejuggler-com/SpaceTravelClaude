#version 460 core

#include "common/math.glsl"
#include "common/noise.glsl"

// Debris shards thrown out by the explosion: each rides outward with the shell at its own speed, drawn as a
// streak from where it was a moment ago to where it is now (6 vertices per shard, built in screen space).

uniform mat4 uViewProj;
uniform vec2 uResolution;
uniform float uRadius;          // shell radius now
uniform float uPreviousRadius;  // shell radius a moment ago
uniform float uBrightness;

out vec2 vLocal;
out vec3 vColor;
out float vLength;

void main() {
    uint id = uint(gl_VertexID / 6);
    vec2 corner = quadCorner(gl_VertexID % 6);
    vec3 dir = normalize(vec3(hashU(id, 1u), hashU(id, 2u), hashU(id, 3u)) * 2.0 - 1.0 + 1e-4);
    float speed = mix(0.7, 1.2, hashU(id, 4u));

    vec4 head = uViewProj * vec4(dir * uRadius * speed, 1.0);
    vec4 tail = uViewProj * vec4(dir * uPreviousRadius * speed, 1.0);
    if (head.w < 0.1 || tail.w < 0.1) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);  // behind the camera: off screen
        return;
    }
    vec2 halfRes = 0.5 * uResolution;
    vec2 h = head.xy / head.w * halfRes;
    vec2 t = tail.xy / tail.w * halfRes;
    vec2 axis = h - t;
    float len = length(axis);
    axis = len > 1e-3 ? axis / len : vec2(1.0, 0.0);
    vec2 perp = vec2(-axis.y, axis.x);
    float width = mix(0.8, 2.2, hashU(id, 5u));

    vec2 end = corner.x < 0.0 ? t - axis * width : h + axis * width;
    gl_Position = vec4((end + perp * corner.y * width) / halfRes, 0.0, 1.0);
    vLength = 0.5 * len / width;
    vLocal = vec2(corner.x * (vLength + 1.0), corner.y);
    vColor = mix(vec3(1.0, 0.55, 0.2), vec3(1.0, 0.92, 0.75), hashU(id, 6u)) * uBrightness / (1.0 + vLength * 0.1);
}
