#version 460 core

#include "common/camera.glsl"
#include "common/math.glsl"
#include "common/noise.glsl"

// Supernova, volume pass (reduced resolution).
// Before the flash: a pulsating red supergiant (huge convection cells from the cell noise, limb darkening) inside
// a glowing envelope of lost gas. After: a raymarched shock shell — wrinkled front (Rayleigh-Taylor fingers),
// hot blue-white at the front, pink (hydrogen) and teal (oxygen) cooling filaments inside, its noise scaled with
// the radius so the structure expands with it — plus an equatorial ring flash and a pulsar sweeping its beams.
// Output: rgb = light, a = 1 - transmittance.

in vec2 vUV;
out vec4 fragColor;

uniform vec2 uResolution;
uniform float uTime;
uniform sampler3D uNoise;
uniform int uSteps;
uniform float uStarRadius;     // 0 once collapsed
uniform float uInstability;    // 0..1 toward the collapse
uniform float uShellRadius;    // 0 before the flash
uniform float uShellAge;       // seconds since the flash
uniform float uShellBrightness;
uniform float uRingRadius;
uniform float uRingIntensity;
uniform float uPulsar;         // 0..1
uniform float uPulsarAngle;

const vec3 kRingNormal = normalize(vec3(0.25, 1.0, 0.1));
const vec3 kSpinAxis = normalize(vec3(-0.3, 1.0, 0.2));

vec3 starSurface(vec3 n, vec3 rd) {
    float c = cos(uTime * 0.03), s = sin(uTime * 0.03);
    vec3 q = vec3(c * n.x + s * n.z, n.y, -s * n.x + c * n.z);
    float cells = texture(uNoise, q * 0.45 + vec3(0.0, uTime * 0.004, 0.0)).g;
    float detail = texture(uNoise, q * 1.6).r;
    float heat = cells * 0.75 + detail * 0.25;
    vec3 col = mix(vec3(0.3, 0.02, 0.0), vec3(1.0, 0.25, 0.05), smoothstep(0.3, 0.75, heat));  // cool red giant
    col = mix(col, vec3(1.0, 0.55, 0.2), smoothstep(0.75, 0.95, heat));
    float mu = max(dot(n, -rd), 0.0);
    float flicker = 1.0 + uInstability * (0.6 * sin(uTime * 9.0 + detail * 20.0) + 0.8);
    return col * (0.25 + 0.75 * pow(mu, 0.7)) * 1.6 * flicker;
}

float shellDensity(vec3 p, out float heat, out float oxygen) {
    float r = length(p);
    vec3 dir = p / max(r, 1e-4);
    float wrinkle = texture(uNoise, dir * 0.9 + vec3(0.13, 0.71, 0.37)).r - 0.5;
    float front = uShellRadius * (1.0 + 0.2 * wrinkle);
    float x = (r - front) / (uShellRadius * 0.22 + 1.0);  // 0 at the front, negative inside
    float profile = smoothstep(0.15, 0.0, x) * smoothstep(-1.4, -0.2, x);
    vec3 q = p / max(uShellRadius, 1.0);                  // self-similar: structure expands with the shell
    float filaments = texture(uNoise, q * 1.6 + vec3(0.2, 0.5, 0.1)).g;
    float fine = texture(uNoise, q * 4.5).r;
    heat = smoothstep(-0.5, 0.1, x);
    oxygen = texture(uNoise, q * 2.3 + 0.61).r;
    return profile * (0.06 + smoothstep(0.55, 0.85, filaments) * 1.6) * (0.6 + 0.6 * fine);
}

void main() {
    vec3 ro = uCamPos;
    vec3 rd = cameraRay(gl_FragCoord.xy, uResolution);
    vec3 col = vec3(0.0);
    float transmittance = 1.0;

    // The star and its envelope.
    float tStar = 1e9;
    if (uStarRadius > 0.0) {
        float b = dot(ro, rd);
        float h = b * b - (dot(ro, ro) - uStarRadius * uStarRadius);
        float tc = max(-b, 0.0);
        float impact = length(ro + rd * tc);
        float envelope = exp(-max(impact - uStarRadius, 0.0) / (uStarRadius * 0.35)) * (0.6 + 0.5 * uInstability);
        col += vec3(1.0, 0.3, 0.08) * envelope * 0.8;
        if (h > 0.0 && -b - sqrt(h) > 0.0) {
            tStar = -b - sqrt(h);
            col += starSurface(normalize(ro + rd * tStar), rd);
            transmittance = 0.0;
        }
    }

    // The shock shell, ring and pulsar beams.
    if (uShellRadius > 0.0) {
        float bound = uShellRadius * 1.35 + 5.0;
        float b = dot(ro, rd);
        float h = b * b - (dot(ro, ro) - bound * bound);
        if (h > 0.0) {
            h = sqrt(h);
            float t0 = max(-b - h, 0.0);
            float t1 = min(-b + h, tStar);
            float dt = (t1 - t0) / float(uSteps);
            float t = t0 + dt * hash12(gl_FragCoord.xy);
            vec3 spin = kSpinAxis;
            vec3 beamAxis = normalize(spin * 0.4 + cross(spin, vec3(cos(uPulsarAngle), 0.0, sin(uPulsarAngle))));
            for (int i = 0; i < uSteps && transmittance > 0.02; ++i, t += dt) {
                vec3 p = ro + rd * t;
                float heat, oxygen;
                float density = shellDensity(p, heat, oxygen);
                vec3 cool = mix(vec3(1.0, 0.28, 0.42), vec3(0.2, 0.8, 0.8), smoothstep(0.45, 0.7, oxygen));
                vec3 emission = mix(cool, vec3(0.7, 0.82, 1.0) * 3.0, heat) * density * uShellBrightness;

                // Equatorial ring.
                float ringHeight = dot(p, kRingNormal);
                float ringRadial = length(p - kRingNormal * ringHeight) - uRingRadius;
                emission += vec3(0.9, 0.85, 1.0) * exp(-(ringRadial * ringRadial + ringHeight * ringHeight) / 6.0) * uRingIntensity;

                // Pulsar beams: two narrow cones along the sweeping beam axis.
                float r = length(p);
                float alignment = abs(dot(p / max(r, 1e-3), beamAxis));
                emission += vec3(0.6, 0.75, 1.0) * pow(alignment, 600.0) * exp(-r / 50.0) * 4.0 * uPulsar;

                col += transmittance * emission * dt * 0.012;
                transmittance *= exp(-density * dt * 0.012);
            }
        }
        // The neutron star itself.
        float tc = max(-dot(ro, rd), 0.0);
        float impact = length(ro + rd * tc);
        col += transmittance * vec3(0.7, 0.8, 1.0) * (exp(-impact * 2.0) * 6.0 + exp(-impact * 0.4) * 0.3) * uPulsar;
    }
    fragColor = vec4(col, 1.0 - transmittance);
}
