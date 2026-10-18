#version 460 core

#include "common/camera.glsl"
#include "common/math.glsl"
#include "common/noise.glsl"
#include "common/sky.glsl"
#include "scenes/rings/rings.glsl"

// Ringed giant, raytraced pass: sky, banded planet (storms, polar cap, rim glow, ring shadows, ringshine) and
// the ring disc (planet shadow, lit or backlit face). Writes depth for the planet and dense ring parts so the
// rasterized ice chunks near the camera are hidden correctly. The ring sheet fades out close to the camera,
// where the chunks take over.

in vec2 vUV;
out vec4 fragColor;

uniform vec2 uResolution;
uniform float uTime;
uniform mat4 uViewProj;
uniform vec3 uLightDir;
uniform vec3 uSunColor;
uniform float uNearFade;   // distance below which the ring sheet gives way to the chunks

float depthOf(vec3 p) {
    vec4 clip = uViewProj * vec4(p, 1.0);
    return clamp(clip.z / clip.w * 0.5 + 0.5, 0.0, 1.0);
}

vec3 planetColor(vec3 n, vec3 p, vec3 rd) {
    vec3 L = uLightDir;
    float c = cos(uTime * 0.01), s = sin(uTime * 0.01);
    vec3 q = vec3(c * n.x + s * n.z, n.y, -s * n.x + c * n.z);
    float turbulence = fbm(q * 4.0);
    float band = sin(n.y * 22.0 + turbulence * 3.0) * 0.5 + 0.5;
    vec3 albedo = mix(vec3(0.74, 0.62, 0.45), vec3(0.93, 0.87, 0.74), band);
    albedo = mix(albedo, vec3(0.6, 0.44, 0.3), smoothstep(0.6, 0.8, fbm(q * 9.0 + 3.0)) * 0.5);  // storms
    albedo *= mix(1.0, 0.7, smoothstep(0.8, 0.95, abs(n.y)));                                  // polar caps

    float ndl = dot(n, L);
    vec3 col = albedo * uSunColor * max(ndl, 0.0) * ringShadow(p, L);
    col += albedo * uSunColor * 0.025 * max(-ndl, 0.0) * smoothstep(0.0, 0.5, abs(n.y));  // ringshine at night
    float mu = max(dot(n, -rd), 0.0);
    col *= mix(0.6, 1.0, mu);  // limb darkening
    col += vec3(0.55, 0.68, 0.95) * pow(1.0 - mu, 3.0) * smoothstep(-0.2, 0.4, ndl) * 0.6;
    return col;
}

vec3 ringColor(vec3 p, float r, float opacity, vec3 ro, vec3 rd) {
    vec3 L = uLightDir;
    vec3 albedo = ringAlbedo(r);
    float lit = planetShadow(p, L);
    bool sameSide = (ro.y > 0.0) == (L.y > 0.0);
    vec3 face = sameSide ? albedo * uSunColor * (0.25 + 0.75 * abs(L.y))
                         // seen from the dark side: light diffusing through thin parts, strongest toward the sun
                         : albedo * uSunColor * (1.0 - opacity) * (0.15 + 1.4 * pow(max(dot(rd, L), 0.0), 3.0));
    return face * lit;
}

void main() {
    vec3 ro = uCamPos;
    vec3 rd = cameraRay(gl_FragCoord.xy, uResolution);
    vec3 col = vec3(0.001, 0.001, 0.002) + starSky(rd, 1.0 / (uResolution.y * uFocal)) * 0.7;
    float sunAngle = acos(clamp(dot(rd, uLightDir), -1.0, 1.0));
    col += uSunColor * (smoothstep(0.006, 0.004, sunAngle) * 60.0 + exp(-sunAngle * 25.0) * 0.6);
    float depth = 1.0;

    float b = dot(ro, rd);
    float h = b * b - (dot(ro, ro) - uPlanetRadius * uPlanetRadius);
    float tPlanet = h > 0.0 ? -b - sqrt(h) : -1.0;
    if (tPlanet > 0.0) {
        vec3 p = ro + rd * tPlanet;
        col = planetColor(normalize(p), p, rd);
        depth = depthOf(p);
    }

    float tRing = abs(rd.y) > 1e-6 ? -ro.y / rd.y : -1.0;
    if (tRing > 0.0 && (tPlanet < 0.0 || tRing < tPlanet)) {
        vec3 p = ro + rd * tRing;
        float r = length(p.xz);
        float opacity = ringOpacity(r) * smoothstep(uNearFade * 0.35, uNearFade, tRing);
        if (opacity > 0.0) {
            col = mix(col, ringColor(p, r, opacity, ro, rd), opacity);
            if (opacity > 0.5) depth = depthOf(p);
        }
    }

    gl_FragDepth = depth;
    fragColor = vec4(col, 1.0);
}
