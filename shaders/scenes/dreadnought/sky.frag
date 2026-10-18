#version 460 core

#include "common/camera.glsl"
#include "common/math.glsl"
#include "common/noise.glsl"
#include "common/sky.glsl"

// Backdrop for the dreadnought: stars, the sun, and a huge planet below (the ship flies in low orbit).
// Planet: continents and oceans with sun glint, drifting clouds, night-side city lights, atmospheric haze
// toward the limb, and a thin glowing atmosphere line above the horizon.

in vec2 vUV;
out vec4 fragColor;

uniform vec2 uResolution;
uniform float uTime;
uniform vec3 uLightDir;
uniform vec3 uSunColor;
uniform vec3 uPlanetCenter;
uniform float uPlanetRadius;
uniform float uAtmosphere;   // glow scale height above the surface
uniform float uNightLights;

const vec3 kAtmosphereColor = vec3(0.3, 0.55, 1.0);

vec3 planetSurface(vec3 n, vec3 rd) {
    float c = cos(uTime * 0.004), s = sin(uTime * 0.004);
    vec3 q = vec3(c * n.x + s * n.z, n.y, -s * n.x + c * n.z);  // slow planet rotation

    float land = fbm(q * 30.0);
    float continents = smoothstep(0.52, 0.56, land);
    vec3 ground = mix(vec3(0.08, 0.1, 0.05), vec3(0.22, 0.18, 0.12), fbm(q * 90.0));
    vec3 albedo = mix(vec3(0.01, 0.03, 0.08), ground, continents);
    float clouds = smoothstep(0.5, 0.78, fbm(q * 60.0 + vec3(uTime * 0.002, 0.0, 0.0)));
    albedo = mix(albedo, vec3(0.85), clouds);

    float ndl = dot(n, uLightDir);
    vec3 col = albedo * uSunColor * max(ndl, 0.0) * 0.9;
    col += uSunColor * pow(max(dot(reflect(rd, n), uLightDir), 0.0), 300.0) * (1.0 - continents) * (1.0 - clouds) * 3.0;

    float night = smoothstep(0.05, -0.15, ndl);
    float cities = smoothstep(0.7, 0.9, valueNoise(q * 900.0)) * smoothstep(0.55, 0.6, land);
    col += vec3(1.0, 0.65, 0.3) * cities * night * (1.0 - clouds) * uNightLights;

    float haze = pow(1.0 - max(dot(n, -rd), 0.0), 4.0);
    return mix(col, kAtmosphereColor * smoothstep(-0.2, 0.3, ndl) * 1.2, haze * 0.85);
}

void main() {
    vec3 ro = uCamPos;
    vec3 rd = cameraRay(gl_FragCoord.xy, uResolution);
    float pixelAngle = 1.0 / (uResolution.y * uFocal);

    vec3 col = vec3(0.001, 0.0012, 0.0025) + starSky(rd, pixelAngle) * 0.6;
    float angle = acos(clamp(dot(rd, uLightDir), -1.0, 1.0));
    col += uSunColor * (smoothstep(0.006 + pixelAngle, 0.006 - pixelAngle, angle) * 80.0 + exp(-angle * 30.0) * 1.5 +
                        exp(-angle * 5.0) * 0.08);

    vec3 oc = ro - uPlanetCenter;
    float b = dot(oc, rd);
    float h = b * b - (dot(oc, oc) - uPlanetRadius * uPlanetRadius);
    if (h > 0.0 && -b - sqrt(h) > 0.0) {
        vec3 n = normalize(oc + rd * (-b - sqrt(h)));
        col = planetSurface(n, rd);
    } else if (-b > 0.0) {
        // Ray passes above the horizon: glow from the thin atmosphere, brightest on the day side.
        vec3 closest = oc - rd * b;
        float altitude = length(closest) - uPlanetRadius;
        float daySide = smoothstep(-0.2, 0.3, dot(normalize(closest), uLightDir));
        col += kAtmosphereColor * exp(-max(altitude, 0.0) / uAtmosphere) * daySide * 1.6;
    }
    fragColor = vec4(col, 1.0);
}
