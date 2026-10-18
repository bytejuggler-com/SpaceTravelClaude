#version 460 core

#include "common/math.glsl"
#include "common/noise.glsl"

// 3D warp starfield. Stars fill a hollow cylinder around the view axis and wrap in depth. Position is a pure
// function of the distance travelled, so the effect can be scrubbed. Each star is a quad stretched from where
// it was uShutter seconds ago (tail) to where it is now (head): motion blur that becomes warp streaks.

uniform vec2 uResolution;
uniform float uTravel;   // world units travelled along the view axis
uniform float uSpeed;    // current speed (units / s)
uniform float uShutter;  // motion-blur exposure (s)
uniform float uDepth;    // depth of the star volume (units)
uniform float uRoll;     // camera roll (radians)
uniform float uFocal;
uniform float uHyper;    // 0..1 hyperspace amount: tints stars blue
uniform float uKick;

out vec2 vLocal;     // x along the streak, y across, in units of the half-width
out vec3 vColor;
out float vLength;   // streak half-length in units of the half-width

const float kNear = 0.4;

void main() {
    int star = gl_VertexID / 6;
    vec2 corner = quadCorner(gl_VertexID % 6);
    float id = float(star);
    float h1 = hash12(vec2(id, 1.7));
    float h2 = hash12(vec2(id, 5.3));
    float h3 = hash12(vec2(id, 9.1));
    float h4 = hash12(vec2(id, 13.9));

    float angle = h1 * TAU;
    float radius = sqrt(mix(0.04, 30.0, h2));  // uniform over the disc area, with a small empty core
    vec2 xy = rotate2d(uRoll) * (vec2(cos(angle), sin(angle)) * radius);

    float z = mod(h3 * uDepth - uTravel, uDepth) + kNear;
    float zTail = z + uSpeed * uShutter;

    vec2 head = xy / z * uFocal;
    vec2 tail = xy / zTail * uFocal;
    float pixel = 2.0 / uResolution.y;
    float halfWidth = max(0.01 / z, 0.8 * pixel);

    vec2 axis = head - tail;
    float len = length(axis);
    axis = len > 1e-6 ? axis / len : vec2(1.0, 0.0);
    vec2 perp = vec2(-axis.y, axis.x);

    vec2 end = corner.x < 0.0 ? tail - axis * halfWidth : head + axis * halfWidth;
    vec2 pos = end + perp * corner.y * halfWidth;
    float aspect = uResolution.x / uResolution.y;
    gl_Position = vec4(pos.x / aspect, pos.y, 0.0, 1.0);

    vLength = 0.5 * len / halfWidth;
    vLocal = vec2(corner.x * (vLength + 1.0), corner.y);

    float fadeIn = smoothstep(uDepth + kNear, uDepth * 0.6, z);  // appear softly in the distance
    vec3 tint = h4 < 0.7 ? vec3(0.85, 0.9, 1.0) : (h4 < 0.85 ? vec3(1.0, 0.85, 0.6) : vec3(0.6, 0.75, 1.0));
    tint = mix(tint, vec3(0.55, 0.7, 1.0), uHyper);
    float energy = 1.0 / (1.0 + vLength * 0.12);  // long streaks spread their light
    vColor = tint * (1.2 / z + 0.1) * fadeIn * energy * (1.0 + 0.6 * uKick);
}
