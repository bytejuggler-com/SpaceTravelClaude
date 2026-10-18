#version 460 core

#include "common/camera.glsl"
#include "common/math.glsl"
#include "common/noise.glsl"

// Nebula crossing, volume pass (reduced resolution). Dark dust pillars rising from a dusty floor, eroded by the
// shared noise volume, absorb and scatter light from a star cluster behind them: each sample marches a few
// steps toward the cluster for its shadowing, so light shafts, silver linings and bright ionization rims all
// come out of the integration. Glowing gas in Hubble-palette colors fills the space around.
// Output: rgb = light reaching the camera from the volume, a = 1 - transmittance.

in vec2 vUV;
out vec4 fragColor;

uniform vec2 uResolution;
uniform float uTime;
uniform sampler3D uNoise;
uniform vec3 uLightPos;     // the star cluster
uniform vec3 uLightColor;
uniform int uSteps;
uniform int uLightSteps;
uniform float uDust;        // dust extinction multiplier
uniform float uGas;         // gas emission multiplier
uniform float uScatter;     // dust scattering albedo
uniform float uRim;         // ionization rim brightness
uniform float uFar;

const float kDustSigma = 0.22;
const int kPillars = 3;
const vec3 kNebulaCenter = vec3(0.0, 40.0, -120.0);  // the glowing gas is centered on the pillars

struct Pillar {
    vec3 base;
    float height;
    float baseRadius;
    float topRadius;
    float phase;
};

Pillar pillar(int i) {
    if (i == 0) return Pillar(vec3(-40.0, -90.0, 0.0), 175.0, 30.0, 13.0, 0.0);
    if (i == 1) return Pillar(vec3(15.0, -90.0, -35.0), 140.0, 24.0, 10.0, 1.7);
    return Pillar(vec3(62.0, -90.0, 18.0), 105.0, 20.0, 9.0, 3.1);
}

// Signed distance to the smooth pillar shapes (bent, tapering columns with rounded heads) and the dust floor.
float shapeDistance(vec3 p) {
    float d = p.y + 72.0;
    for (int i = 0; i < kPillars; ++i) {
        Pillar c = pillar(i);
        float h = p.y - c.base.y;
        float t = clamp(h / c.height, 0.0, 1.0);
        vec2 axis = c.base.xz + vec2(sin(h * 0.018 + c.phase) * 9.0, cos(h * 0.013 + c.phase) * 6.0);
        float radius = mix(c.baseRadius, c.topRadius, t);
        vec2 offset = p.xz - axis;
        float top = c.base.y + c.height;
        float dist = p.y > top ? length(vec3(offset, (p.y - top) * 1.3)) - radius : length(offset) - radius;
        d = min(d, dist);
    }
    return d;
}

float dustDensity(vec3 p) {
    float d = shapeDistance(p);
    if (d > 26.0) return 0.0;  // beyond the reach of the erosion below
    float clouds = texture(uNoise, p * 0.011).r;
    float billows = texture(uNoise, p * 0.029 + vec3(0.0, uTime * 0.0004, 0.0)).g;
    d += (clouds - 0.5) * 30.0 - (billows - 0.5) * 14.0;
    return clamp(-d * 0.12, 0.0, 1.0);
}

float gasDensity(vec3 p) {
    float clouds = texture(uNoise, p * 0.0032 + vec3(0.0, 0.0, uTime * 0.0006)).r;
    float billows = texture(uNoise, p * 0.009).g;
    float cloud = smoothstep(0.42, 0.75, clouds * 0.75 + billows * 0.35);
    // Behind the pillars only, so the camera's side stays clear and the glow reads as a backdrop.
    return cloud * smoothstep(420.0, 120.0, length(p - kNebulaCenter)) * smoothstep(60.0, -80.0, p.z);
}

// Hubble palette: oxygen teal toward the cluster, hydrogen/sulfur gold-red further out.
vec3 gasColor(vec3 p) {
    return mix(vec3(0.15, 0.75, 0.78), vec3(1.0, 0.48, 0.16), smoothstep(220.0, 420.0, length(p - uLightPos)));
}

// Henyey-Greenstein phase function, scaled so isotropic scattering = 1.
float phaseHG(float cosTheta, float g) {
    float g2 = g * g;
    return (1.0 - g2) / pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5);
}

float lightTransmittance(vec3 p, vec3 L, float distanceToLight) {
    float stepLength = min(distanceToLight, 90.0) / float(uLightSteps);
    float depth = 0.0;
    for (int k = 0; k < uLightSteps; ++k) depth += dustDensity(p + L * (float(k) + 0.5) * stepLength);
    return exp(-depth * stepLength * kDustSigma * uDust * 0.6);
}

void main() {
    vec3 ro = uCamPos;
    vec3 rd = cameraRay(gl_FragCoord.xy, uResolution);

    float dt = uFar / float(uSteps);
    float t = dt * hash12(gl_FragCoord.xy);  // per-pixel jitter turns step banding into fine grain
    vec3 col = vec3(0.0);
    float transmittance = 1.0;

    for (int i = 0; i < uSteps && transmittance > 0.02; ++i, t += dt) {
        vec3 p = ro + rd * t;
        float dust = dustDensity(p);
        float gas = gasDensity(p) * uGas;
        if (dust + gas < 0.002) continue;

        vec3 toLight = uLightPos - p;
        float lightDistance = length(toLight);
        vec3 L = toLight / lightDistance;
        float lit = lightTransmittance(p, L, lightDistance);
        float falloff = 1.0 / (1.0 + (lightDistance / 250.0) * (lightDistance / 250.0));
        float phase = phaseHG(dot(rd, L), 0.35);

        float sigma = dust * kDustSigma * uDust + gas * 0.004;
        // The thin gas scatters little (albedo ~0.15): it glows mostly by its own emission.
        vec3 scattered = uLightColor * lit * falloff * phase * (dust * uScatter * vec3(0.55, 0.4, 0.3) + gas * 0.0006);
        vec3 emitted = gasColor(p) * gas * 0.004 * (0.4 + 0.6 * lit);
        float rim = dust * (1.0 - dust) * 4.0 * lit * falloff * uRim;  // thin bright skin on lit dust edges
        vec3 source = scattered + emitted + vec3(1.0, 0.7, 0.45) * rim * 0.05;

        float stepTransmittance = exp(-sigma * dt);
        col += transmittance * source * (sigma > 1e-5 ? (1.0 - stepTransmittance) / sigma : dt);
        transmittance *= stepTransmittance;
    }
    fragColor = vec4(col, 1.0 - transmittance);
}
