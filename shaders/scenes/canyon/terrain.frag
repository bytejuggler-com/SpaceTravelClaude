#version 460 core

#include "common/camera.glsl"
#include "common/math.glsl"
#include "common/noise.glsl"
#include "scenes/canyon/terrain.glsl"

// Canyon at dusk, terrain pass (reduced resolution): heightfield raymarch, banded rock strata, sandy floors,
// cliff streaks, soft terrain shadows from a low sun, the fighter's shadow (analytic ellipsoid), haze, and
// low volumetric dust lit through the gaps between the cliffs (light shafts). Writes depth for the fighter.

in vec2 vUV;
out vec4 fragColor;

uniform vec2 uResolution;
uniform float uTime;
uniform mat4 uViewProj;
uniform vec3 uLightDir;
uniform vec3 uSunColor;
uniform int uSteps;
uniform float uFar;
uniform float uDust;
uniform float uHaze;
uniform vec3 uShipPos;
uniform mat3 uShipBasis;   // columns: ship x, up, forward
uniform vec3 uPlanetDir;

const vec3 kHorizon = vec3(0.85, 0.45, 0.25);
const vec3 kZenith = vec3(0.05, 0.08, 0.2);
const vec3 kSkyAmbient = vec3(0.25, 0.32, 0.45);

// Ringed giant hanging in the sky (far away, so the camera position does not matter).
vec3 skyPlanet(vec3 rd, vec3 background) {
    vec3 center = uPlanetDir * 4000.0;
    const float radius = 900.0;
    const vec3 ringNormal = normalize(vec3(0.3, 1.0, 0.2));
    vec3 col = background;

    float tRing = dot(center, ringNormal) / dot(rd, ringNormal);
    float b = dot(rd, center);
    float h = b * b - (dot(center, center) - radius * radius);
    float tPlanet = h > 0.0 ? b - sqrt(h) : 1e9;

    if (tPlanet < 1e9) {
        vec3 n = normalize(rd * tPlanet - center);
        float band = sin(n.y * 18.0 + terrainNoise(n.xz * 0.6) * 4.0) * 0.5 + 0.5;
        vec3 albedo = mix(vec3(0.75, 0.65, 0.5), vec3(0.95, 0.88, 0.75), band);
        col = mix(background, albedo * (0.08 + 1.2 * max(dot(n, uLightDir), 0.0)), 0.85);
    }
    if (tRing > 0.0 && tRing < tPlanet) {
        vec3 q = rd * tRing - center;
        float r = length(q) / radius;
        float opacity = (r > 1.4 && r < 2.5) ? (0.45 + 0.35 * sin(r * 40.0)) * smoothstep(1.4, 1.5, r) * smoothstep(2.5, 2.35, r) : 0.0;
        col = mix(col, vec3(0.9, 0.85, 0.75) * 0.8, opacity * 0.75);
    }
    return mix(col, background, exp(-max(rd.y, 0.0) * 6.0) * 0.8);  // sinks into the horizon haze
}

vec3 skyColor(vec3 rd) {
    float sunAmount = max(dot(rd, uLightDir), 0.0);
    vec3 col = mix(kHorizon, kZenith, pow(max(rd.y, 0.0), 0.45));
    col = skyPlanet(rd, col);
    col += uSunColor * (pow(sunAmount, 8.0) * 0.4 + pow(sunAmount, 300.0) * 4.0 + smoothstep(0.9995, 0.9998, sunAmount) * 30.0);
    return col;
}

// Fraction of sunlight not blocked by the fighter (ray toward the sun vs. the ship's bounding ellipsoid).
float shipShadow(vec3 p) {
    vec3 radii = vec3(2.6, 0.5, 2.3);
    mat3 toShip = transpose(uShipBasis);
    vec3 o = (toShip * (p - uShipPos)) / radii;
    vec3 d = (toShip * uLightDir) / radii;
    float s = -dot(o, d) / dot(d, d);
    if (s <= 0.0) return 1.0;
    return mix(0.25, 1.0, smoothstep(0.8, 1.15, length(o + d * s)));
}

vec3 rockColor(vec3 p, vec3 n) {
    float strata = p.y * 0.22 + terrainNoise(p.xz * 0.01) * 3.0;
    vec3 band = mix(vec3(0.55, 0.22, 0.12), vec3(0.75, 0.42, 0.22), smoothstep(0.3, 0.7, fract(strata * 0.37)));
    band = mix(band, vec3(0.82, 0.68, 0.5), smoothstep(0.75, 0.95, fract(strata * 0.21)) * 0.8);
    band = mix(band, vec3(0.35, 0.2, 0.14), smoothstep(0.8, 0.95, fract(strata * 0.53)) * 0.7);
    float flatness = smoothstep(0.6, 0.9, n.y);
    vec3 albedo = mix(band, vec3(0.78, 0.6, 0.42), flatness);
    float streaks = smoothstep(0.5, 0.8, texture(uNoise, vec3((p.x + p.z) * 0.05, p.y * 0.004, 0.83)).r) * (1.0 - flatness);
    return albedo * (1.0 - 0.45 * streaks);
}

float phaseHG(float cosTheta, float g) {
    float g2 = g * g;
    return (1.0 - g2) / pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5);
}

// Low dust in the canyon, lit by the sun where it clears the cliffs.
vec4 canyonDust(vec3 ro, vec3 rd, float tMax) {
    const int kSteps = 16;
    float tEnd = min(tMax, 260.0);
    float dt = tEnd / float(kSteps);
    float t = dt * hash12(gl_FragCoord.xy);
    vec3 col = vec3(0.0);
    float transmittance = 1.0;
    float phase = phaseHG(dot(rd, uLightDir), 0.6);
    for (int i = 0; i < kSteps; ++i, t += dt) {
        vec3 p = ro + rd * t;
        float density = exp(-max(p.y, 0.0) / 12.0) *
                        (0.4 + texture(uNoise, p * 0.012 + vec3(uTime * 0.01, 0.0, uTime * 0.004)).r) * uDust * 0.012;
        float lit = 1.0;
        for (int k = 1; k <= 2; ++k) {
            float s = k == 1 ? 12.0 : 40.0;
            vec3 q = p + uLightDir * s;
            lit *= clamp((q.y - terrainHeight(q.xz)) * 0.2, 0.0, 1.0);
        }
        vec3 inscatter = uSunColor * lit * phase * 0.8 + kSkyAmbient * 0.15;
        col += transmittance * inscatter * density * dt;
        transmittance *= exp(-density * dt);
    }
    return vec4(col, transmittance);
}

void main() {
    vec3 ro = uCamPos;
    vec3 rd = cameraRay(gl_FragCoord.xy, uResolution);

    // Heightfield march: conservative steps (steep cliffs), then a short bisection refine.
    float t = 0.5;
    float previous = t;
    bool hit = false;
    for (int i = 0; i < uSteps; ++i) {
        vec3 p = ro + rd * t;
        float h = p.y - terrainHeight(p.xz);
        if (h < 0.002 * t) {
            hit = true;
            break;
        }
        previous = t;
        t += max(h * 0.3, 0.05 + 0.002 * t);
        if (t > uFar) break;
    }
    if (hit) {
        float lo = previous, hi = t;
        for (int i = 0; i < 5; ++i) {
            float mid = 0.5 * (lo + hi);
            vec3 p = ro + rd * mid;
            if (p.y - terrainHeight(p.xz) < 0.0) hi = mid;
            else lo = mid;
        }
        t = hi;
    }

    vec3 col;
    float depth = 1.0;
    if (hit) {
        vec3 p = ro + rd * t;
        float e = 0.15 + 0.002 * t;
        vec3 n = normalize(vec3(terrainHeight(p.xz - vec2(e, 0.0)) - terrainHeight(p.xz + vec2(e, 0.0)), 2.0 * e,
                                terrainHeight(p.xz - vec2(0.0, e)) - terrainHeight(p.xz + vec2(0.0, e))));
        vec3 albedo = rockColor(p, n);
        float sun = max(dot(n, uLightDir), 0.0) * terrainShadow(p + n * 0.3, uLightDir) * shipShadow(p);
        float depthInCanyon = mix(0.5, 1.0, smoothstep(0.0, 30.0, p.y));
        col = albedo * (uSunColor * sun + kSkyAmbient * (0.5 + 0.5 * n.y) * 0.25 * depthInCanyon +
                        vec3(0.6, 0.35, 0.2) * 0.12 * (1.0 - n.y) * depthInCanyon);
        vec3 haze = mix(kHorizon, uSunColor * 0.6, pow(max(dot(rd, uLightDir), 0.0), 8.0));
        col = mix(col, haze, 1.0 - exp(-t * 0.0015 * uHaze));
        vec4 clip = uViewProj * vec4(p, 1.0);
        depth = clamp(clip.z / clip.w * 0.5 + 0.5, 0.0, 1.0);
    } else {
        col = skyColor(rd);
        t = uFar;
    }

    vec4 dust = canyonDust(ro, rd, t);
    col = col * dust.a + dust.rgb;

    gl_FragDepth = depth;
    fragColor = vec4(col, 1.0);
}
