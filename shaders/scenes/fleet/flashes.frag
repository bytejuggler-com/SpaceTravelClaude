#version 460 core

// Fleet arrivals, added on top of the frame: for each ship dropping out of hyperspace, a hyperspace streak (a
// glowing line from where it came from to where it is) and an arrival flash with a horizontal flare.
// Positions are in pixels, computed on the CPU.

in vec2 vUV;
out vec4 fragColor;

const int kMaxShips = 12;

uniform vec2 uResolution;
uniform int uShipCount;
uniform vec4 uStreak[kMaxShips];  // xy = tail, zw = head (pixels)
uniform vec2 uFlash[kMaxShips];   // x = streak intensity, y = flash intensity
uniform vec2 uFlashPos[kMaxShips];

float segmentDistance(vec2 p, vec2 a, vec2 b) {
    vec2 ab = b - a;
    float t = clamp(dot(p - a, ab) / max(dot(ab, ab), 1e-4), 0.0, 1.0);
    return length(p - (a + ab * t));
}

void main() {
    vec2 p = gl_FragCoord.xy;
    float scale = uResolution.y / 1080.0;
    vec3 col = vec3(0.0);
    for (int i = 0; i < uShipCount; ++i) {
        if (uFlash[i].x > 0.001) {
            float d = segmentDistance(p, uStreak[i].xy, uStreak[i].zw) / scale;
            col += vec3(0.6, 0.75, 1.0) * uFlash[i].x * (exp(-d * 0.8) * 2.0 + exp(-d * 0.08) * 0.15);
        }
        if (uFlash[i].y > 0.001) {
            vec2 q = (p - uFlashPos[i]) / scale;
            float r = length(q);
            col += vec3(0.8, 0.9, 1.0) * uFlash[i].y * (exp(-r * 0.05) * 1.5 + exp(-r * 0.01) * 0.2);
            col += vec3(0.5, 0.7, 1.0) * uFlash[i].y * exp(-abs(q.y) * 0.4) * exp(-abs(q.x) * 0.004) * 0.8;  // flare
        }
    }
    fragColor = vec4(col, 1.0);
}
