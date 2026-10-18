#version 460 core

#include "common/math.glsl"
#include "common/noise.glsl"

// Classic Amiga parallax starfield: stars in depth layers scrolling right to left, nearer layers faster,
// bigger and brighter. Each star is a quad (6 vertices) built from gl_VertexID; star k belongs to layer
// k / uStarsPerLayer, so a layer range can be drawn on its own (glDrawArrays first/count).

uniform vec2 uResolution;
uniform float uTravel;       // screen widths travelled by the nearest layer
uniform float uSpeed;        // current speed of the nearest layer (screen widths / s), sets streak length
uniform int uStarsPerLayer;
uniform int uLayers;
uniform float uTime;
uniform float uBeatPulse;

out vec2 vLocal;     // position in the quad in units of the star radius; x runs along the streak
out vec3 vColor;
out float vStreak;   // streak half-length in units of the star radius

void main() {
    int star = gl_VertexID / 6;
    vec2 corner = quadCorner(gl_VertexID % 6);
    int layer = star / uStarsPerLayer;
    float depth = float(layer + 1) / float(uLayers);  // (0, 1], 1 = nearest

    vec2 h = hash22(vec2(float(star) * 1.37, 11.0));
    float h2 = hash12(vec2(float(star), 29.0));

    float aspect = uResolution.x / uResolution.y;
    float parallax = mix(0.06, 1.0, depth * depth);
    float span = 2.0 * aspect + 0.4;  // wrap slightly off-screen on both sides
    float x = fract(h.x - uTravel * parallax * 2.0 * aspect / span) * span - 0.5 * span;
    float y = h.y * 2.0 - 1.0;

    float radiusPx = mix(0.7, 2.6, depth) * (0.7 + 0.6 * h2);
    float streakPx = uSpeed * parallax * uResolution.x / 60.0 * 0.5;  // half the distance moved in 1/60 s
    vec2 halfSizePx = vec2(radiusPx * 2.0 + streakPx, radiusPx * 2.0);

    vec2 center = vec2(x / aspect, y);
    gl_Position = vec4(center + corner * halfSizePx * 2.0 / uResolution, 0.0, 1.0);
    vLocal = corner * halfSizePx / radiusPx;
    vStreak = streakPx / radiusPx;

    float twinkle = 0.75 + 0.25 * sin(uTime * (2.0 + 4.0 * h2) + h.x * 50.0);
    vec3 tint = mix(vec3(0.75, 0.82, 1.0), vec3(1.0, 0.9, 0.75), fract(h2 * 7.0));
    float brightness = mix(0.25, 3.0, depth * depth) * twinkle * (1.0 + 0.5 * uBeatPulse * depth);
    vColor = tint * brightness / (1.0 + vStreak * 0.3);  // fast stars spread their light over the streak
}
