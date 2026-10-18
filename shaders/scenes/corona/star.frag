#version 460 core

#include "common/camera.glsl"
#include "common/math.glsl"
#include "common/noise.glsl"

// Giant star (radius 1 at the origin), rendered at reduced resolution:
//  - boiling surface: domain-warped fbm granulation, fine cells, sunspots, limb darkening
//  - corona: flame streamers flowing outward around the silhouette (evaluated at the ray's closest approach)
//  - prominences: plasma arcs looping off the surface, raymarched as glowing tubes
// Output is premultiplied: rgb = light, a = how much the disc hides what is behind it.

in vec2 vUV;
out vec4 fragColor;

uniform vec2 uResolution;
uniform float uTime;
uniform float uRotation;          // star spin (radians)
uniform float uSurfaceIntensity;
uniform float uCoronaIntensity;
uniform float uCoronaReach;       // typical flame length (star radii)
uniform float uProminences;       // prominence brightness
uniform float uFlare;             // music-driven corona flare, 0..1
uniform float uPulse;             // music-driven surface pulse, 0..1

const int kArcs = 5;
const float kArcBounds = 1.5;     // prominences stay inside this radius

vec3 rotateY(vec3 p, float a) {
    float c = cos(a), s = sin(a);
    return vec3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z);
}

vec3 surface(vec3 n, vec3 rd) {
    vec3 p = rotateY(n, -uRotation);  // into the star's frame (same as the prominences)
    float t = uTime;
    vec3 warp = vec3(fbm(p * 3.0 + vec3(0.0, 0.0, t * 0.05)),
                     fbm(p * 3.0 + vec3(7.1, 0.0, -t * 0.04)),
                     fbm(p * 3.0 + vec3(0.0, 13.7, t * 0.03)));
    float granulation = fbm(p * 9.0 + warp * 2.2 + t * 0.04);
    float cells = valueNoise(p * 30.0 + warp * 4.0 + t * 0.1);
    float spots = smoothstep(0.66, 0.74, fbm(p * 1.7 + 4.0));
    float heat = (granulation * 0.75 + cells * 0.25) * (1.0 - 0.85 * spots);

    vec3 col = mix(vec3(0.8, 0.18, 0.02), vec3(1.0, 0.62, 0.2), smoothstep(0.25, 0.6, heat));
    col = mix(col, vec3(1.0, 0.93, 0.75), smoothstep(0.55, 0.85, heat));

    float mu = max(dot(n, -rd), 0.0);
    col *= 0.3 + 0.7 * pow(mu, 0.55);  // limb darkening
    return col * uSurfaceIntensity * (1.0 + 0.25 * uPulse);
}

vec3 corona(vec3 ro, vec3 rd, out float impact) {
    float tc = -dot(ro, rd);  // ray parameter of the closest approach to the star center
    vec3 closest = ro + rd * tc;
    impact = length(closest);
    if (tc < 0.0) return vec3(0.0);

    vec3 dir = rotateY(closest / max(impact, 1e-4), uRotation * 0.5);
    float height = max(impact - 1.0, 0.0);
    // Shifting the noise domain diagonally with (height - time) stretches it into streamers moving outward.
    float flow = height - uTime * 0.12;
    float flames = fbm(dir * 5.0 + vec3(flow * 0.9)) * 0.7 + fbm(dir * 11.0 + vec3(flow * 1.7) + 3.1) * 0.3;

    float reach = uCoronaReach * (0.35 + 1.3 * flames);
    float glow = exp(-height / max(reach, 1e-3) * 3.0);
    float rim = exp(-height * 30.0);
    float halo = exp(-height * 1.2) * 0.2;

    vec3 col = mix(vec3(1.0, 0.72, 0.35), vec3(1.0, 0.28, 0.08), smoothstep(0.0, 1.2, height));
    return col * ((glow * (0.6 + 1.4 * flames) + rim * 1.5) * (1.0 + uFlare) + halo) * uCoronaIntensity;
}

vec3 prominences(vec3 ro, vec3 rd, float tEnd) {
    // Clip the march to the bounding sphere and stop at the star surface.
    float b = dot(ro, rd);
    float c = dot(ro, ro) - kArcBounds * kArcBounds;
    float h = b * b - c;
    if (h < 0.0) return vec3(0.0);
    h = sqrt(h);
    float t0 = max(-b - h, 0.0);
    float t1 = min(-b + h, tEnd);
    if (t1 <= t0) return vec3(0.0);

    // Arc frames: each arc is a circle centered on a surface point, standing in the plane (u, center).
    vec3 centers[kArcs];
    vec3 us[kArcs];
    vec3 ws[kArcs];
    float heights[kArcs];
    for (int i = 0; i < kArcs; ++i) {
        float fi = float(i);
        vec3 center = normalize(vec3(hash12(vec2(fi, 1.0)), hash12(vec2(fi, 2.0)), hash12(vec2(fi, 3.0))) - 0.5);
        vec3 helper = abs(center.y) < 0.9 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
        vec3 u = normalize(cross(center, helper));
        vec3 w = cross(center, u);
        float spin = hash12(vec2(fi, 4.0)) * TAU;
        u = u * cos(spin) + w * sin(spin);
        centers[i] = center;
        us[i] = u;
        ws[i] = cross(center, u);
        heights[i] = mix(0.18, 0.38, hash12(vec2(fi, 5.0))) * (0.85 + 0.15 * sin(uTime * 0.6 + fi * 1.7));
    }

    const int kSteps = 48;
    float dt = (t1 - t0) / float(kSteps);
    float density = 0.0;
    for (int s = 0; s < kSteps; ++s) {
        vec3 p = rotateY(ro + rd * (t0 + (float(s) + 0.5) * dt), -uRotation);  // into the star's frame
        float nearest = 1e9;
        for (int i = 0; i < kArcs; ++i) {
            vec3 q = p - centers[i];
            vec3 local = vec3(dot(q, us[i]), dot(q, centers[i]), dot(q, ws[i]));
            float along = atan(local.y, local.x);
            float thickness = 0.022 * (0.75 + 0.25 * sin(along * 9.0 + uTime * 2.0 + float(i)));
            float d = length(vec2(length(local.xy) - heights[i], local.z)) - thickness;
            nearest = min(nearest, d);
        }
        nearest = max(nearest, 0.0);
        density += (exp(-nearest * 70.0) + 0.08 * exp(-nearest * 12.0)) * dt;
    }
    return vec3(1.0, 0.32, 0.12) * density * 18.0 * uProminences;
}

void main() {
    vec3 ro = uCamPos;
    vec3 rd = cameraRay(gl_FragCoord.xy, uResolution);

    float impact;
    vec3 col = corona(ro, rd, impact);

    // Star disc with an antialiased edge.
    float b = dot(ro, rd);
    float h = b * b - (dot(ro, ro) - 1.0);
    float edge = max(fwidth(impact), 1e-4) * 1.5;
    float alpha = 1.0 - smoothstep(1.0 - edge, 1.0 + edge, impact);
    float tHit = 1e9;
    if (alpha > 0.0) {
        vec3 n;
        if (h > 0.0) {
            tHit = -b - sqrt(h);
            n = normalize(ro + rd * tHit);
        } else {
            n = normalize(ro - rd * b);  // just outside the edge: use the limb point
        }
        col = mix(col, surface(n, rd), alpha);
    }

    col += prominences(ro, rd, tHit);
    fragColor = vec4(col, alpha);
}
