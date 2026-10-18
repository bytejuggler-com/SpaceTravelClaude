#version 460 core

#include "common/camera.glsl"
#include "common/math.glsl"
#include "common/noise.glsl"
#include "common/sky.glsl"

// Wormhole transit. A spherical mouth (radius uRadius at the origin):
//  - rays through the mouth come out in another universe: a face-on spiral galaxy sky, mapped so the disc shows
//    the whole far sky, compressed and brightened toward the rim (multiple images);
//  - rays passing outside are bent around it (our sky lensed into an Einstein ring), with a thin photon ring;
//  - glowing gas swirls around the mouth (raymarched through the shared noise volume);
// then the plunge (streaking tunnel, light growing at its end) and the exit into the other sky.

in vec2 vUV;
out vec4 fragColor;

uniform vec2 uResolution;
uniform float uTime;
uniform sampler3D uNoise;
uniform float uRadius;
uniform float uLens;
uniform float uGas;
uniform float uTunnel;     // 0..1 blend to the tunnel view
uniform float uExit;       // 0..1 blend to the other sky
uniform float uTravel;     // distance travelled down the tunnel
uniform float uPulse;      // music pulse
uniform vec3 uGalaxyDir;   // where the galaxy sits in the other sky
uniform vec3 uRemnantDir;  // where the supernova remnant sits in our sky

vec3 ourSky(vec3 d, float pixelAngle) {
    vec3 col = vec3(0.002, 0.001, 0.004) + starSky(d, pixelAngle) * 0.8;
    float a = acos(clamp(dot(d, uRemnantDir), -1.0, 1.0));
    float clouds = texture(uNoise, d * 0.8).r;
    float filaments = texture(uNoise, d * 1.7 + 0.3).g;
    col += (vec3(1.0, 0.35, 0.5) * smoothstep(0.45, 0.8, clouds) + vec3(0.2, 0.7, 0.8) * smoothstep(0.55, 0.85, filaments) * 0.6) *
           exp(-a * 3.0) * 0.6;
    return col;
}

vec3 otherSky(vec3 d, float pixelAngle) {
    vec3 col = vec3(0.004, 0.004, 0.01) + starSky(d.zxy, pixelAngle) * 1.2;
    float g = dot(d, uGalaxyDir);
    if (g > 0.0) {
        vec3 U = normalize(cross(uGalaxyDir, vec3(0.3, 1.0, 0.1)));
        vec3 V = cross(uGalaxyDir, U);
        vec2 q = rotate2d(0.3) * (vec2(dot(d, U), dot(d, V)) / g) * 3.0;  // face-on disc in its own plane
        float r = length(q);
        float angle = atan(q.y, q.x);
        float arms = pow(0.5 + 0.5 * cos(2.0 * (angle - log(max(r, 0.02)) * 2.6)), 3.0);
        float grain = texture(uNoise, vec3(q * 1.7, 0.37)).r;
        float disc = exp(-r * 2.2) * (0.35 + 0.65 * arms) * (0.6 + 0.8 * grain);
        vec3 c = mix(vec3(0.5, 0.65, 1.0), vec3(1.0, 0.85, 0.6), exp(-r * 3.0)) * disc * 1.6;
        c += vec3(1.0, 0.85, 0.6) * exp(-r * 9.0) * 3.0;                                                            // core
        c += vec3(1.0, 0.3, 0.55) * smoothstep(0.7, 0.9, texture(uNoise, vec3(q * 4.0, 0.71)).g) * arms * exp(-r * 2.0) * 0.6;  // star-forming knots
        col += c * smoothstep(0.0, 0.2, g);
    }
    return col;
}

vec3 mouthView(vec3 ro, vec3 rd, float pixelAngle, out float tEnter) {
    float tc = -dot(ro, rd);
    vec3 closest = ro + rd * tc;
    float b = length(closest);
    vec3 outward = closest / max(b, 1e-5);
    tEnter = 1e9;
    vec3 col;
    if (tc > 0.0 && b < uRadius) {
        float u = b / uRadius;
        float theta = PI * u * u;  // straight through at the center, looking back behind at the rim
        col = otherSky(normalize(rd * cos(theta) - outward * sin(theta)), pixelAngle) * (1.0 + 1.5 * pow(u, 12.0));
        tEnter = tc - sqrt(uRadius * uRadius - b * b);
    } else {
        float bend = tc > 0.0 ? uLens * 1.2 * uRadius / max(b, uRadius) : 0.0;
        col = ourSky(normalize(rd * cos(bend) - outward * sin(bend)), pixelAngle);
    }
    col += vec3(0.7, 0.8, 1.0) * exp(-abs(b - uRadius) / (0.015 * uRadius)) * 0.8 * step(0.0, tc);  // photon ring
    return col;
}

// Swirling gas in a shell around the mouth (straight rays; the lensing is left to the sky).
vec4 swirlingGas(vec3 ro, vec3 rd, float tEnd) {
    float outer = 3.0 * uRadius;
    float b = dot(ro, rd);
    float h = b * b - (dot(ro, ro) - outer * outer);
    if (h < 0.0 || uGas <= 0.0) return vec4(0.0);
    h = sqrt(h);
    float t0 = max(-b - h, 0.0);
    float t1 = min(-b + h, tEnd);
    if (t1 <= t0) return vec4(0.0);

    const int kSteps = 40;
    float dt = (t1 - t0) / float(kSteps);
    float t = t0 + dt * hash12(gl_FragCoord.xy);
    vec3 col = vec3(0.0);
    float transmittance = 1.0;
    for (int i = 0; i < kSteps; ++i, t += dt) {
        vec3 p = ro + rd * t;
        float r = length(p);
        float shell = smoothstep(outer, 1.2 * uRadius, r) * smoothstep(0.95 * uRadius, 1.15 * uRadius, r);
        if (shell <= 0.0) continue;
        float swirl = uTime * 0.4 + 3.0 * uRadius / r;  // inner gas whirls faster
        vec3 q = vec3(rotate2d(swirl) * p.xy, p.z);
        float n = texture(uNoise, q * 0.06).r * 0.6 + texture(uNoise, q * 0.15 + 0.5).g * 0.4;
        float density = shell * smoothstep(0.45, 0.8, n) * uGas;
        vec3 emission = mix(vec3(0.35, 0.4, 1.0), vec3(0.9, 0.5, 1.0), n) * (1.5 + 3.0 * exp(-(r - uRadius) / (0.2 * uRadius))) *
                        (1.0 + uPulse);
        col += transmittance * emission * density * dt * 0.08;
        transmittance *= exp(-density * dt * 0.05);
    }
    return vec4(col, 1.0 - transmittance);
}

vec3 tunnelView(float pixelAngle) {
    vec2 uv = (gl_FragCoord.xy - 0.5 * uResolution) / uResolution.y;
    float r = length(uv);
    float angle = atan(uv.y, uv.x) / TAU;  // the noise volume repeats with period 1: seamless around the tunnel
    float depth = 0.35 / max(r, 0.01) + uTravel;
    float spiral = angle + depth * 0.05 + uTime * 0.05;
    float n = texture(uNoise, vec3(spiral, depth * 0.15, 0.3)).r;
    float w = texture(uNoise, vec3(spiral * 2.0, depth * 0.4, 0.6)).g;
    vec3 wall = mix(vec3(0.3, 0.35, 1.0), vec3(1.0, 0.4, 0.8), n) * (smoothstep(0.55, 0.85, n) * 2.0 + pow(w, 4.0) * 1.5);
    vec3 end = mix(vec3(0.6, 0.7, 1.0), vec3(1.0), uExit) * exp(-r * (8.0 - 6.0 * uExit)) * (1.0 + 4.0 * uExit);
    return wall * smoothstep(0.02, 0.2, r) * 0.45 * (1.0 + uPulse * 0.5) + end;
}

void main() {
    vec3 ro = uCamPos;
    vec3 rd = cameraRay(gl_FragCoord.xy, uResolution);
    float pixelAngle = 1.0 / (uResolution.y * uFocal);

    float tEnter;
    vec3 col = mouthView(ro, rd, pixelAngle, tEnter);
    vec4 gas = swirlingGas(ro, rd, tEnter);
    col = col * (1.0 - gas.a) + gas.rgb;

    if (uTunnel > 0.0) col = mix(col, tunnelView(pixelAngle), uTunnel);
    if (uExit > 0.0) col = mix(col, otherSky(rd, pixelAngle), uExit);
    fragColor = vec4(col, 1.0);
}
