#version 460 core

#include "common/camera.glsl"
#include "common/math.glsl"
#include "common/noise.glsl"
#include "common/sky.glsl"

// Schwarzschild black hole (event horizon radius 1 at the origin) with a thin accretion disk in the y = 0 plane.
// Each camera ray is integrated as a bent light path: a = -1.5 h^2 x / r^5 (h = |x × v|), the usual
// approximation of null geodesics. That alone yields the shadow, the photon ring, the far side of the disk
// arching over and under the shadow, and the lensed background. Linear HDR output.

in vec2 vUV;
out vec4 fragColor;

uniform vec2 uResolution;
uniform float uTime;
uniform int uMaxSteps;
uniform float uStepScale;    // < 1 = finer steps
uniform float uLensing;      // 0 = straight rays, 1 = real bending
uniform float uInner;        // disk inner radius (3 = innermost stable orbit)
uniform float uOuter;
uniform float uTemp;         // color temperature multiplier
uniform float uBrightness;
uniform float uDoppler;      // 0 = symmetric cinematic disk, 1 = relativistic beaming + shifts
uniform float uDiskSpeed;    // turbulence rotation speed
uniform float uOpacity;
uniform float uJets;
uniform float uBackground;   // nebula band brightness
uniform vec3 uBandNormal;    // orientation of the nebula band on the sky
uniform float uSpotPulse;    // music: hot spots flare, 0..1

const float kHorizon = 1.0;
const int kHotSpots = 3;

// Rough blackbody ramp: 0 = dull red, 1 = orange-white, 2 = blue-white.
vec3 blackbody(float t) {
    vec3 c = vec3(1.0, 0.22, 0.04);
    c = mix(c, vec3(1.0, 0.58, 0.22), smoothstep(0.2, 0.6, t));
    c = mix(c, vec3(1.0, 0.9, 0.78), smoothstep(0.6, 1.1, t));
    c = mix(c, vec3(0.68, 0.78, 1.0), smoothstep(1.1, 1.8, t));
    return c;
}

// Light from the disk where the ray crosses it at p; rayDir is the traced direction (camera -> scene).
// Returns premultiplied color and opacity.
vec4 diskSample(vec3 p, vec3 rayDir) {
    float r = length(p.xz);

    // Thin-disk temperature profile, normalized to peak at 1.
    float x = uInner / r;
    float temperature = pow(x, 0.75) * pow(max(1.0 - sqrt(x), 0.0), 0.25) / 0.488;

    // Turbulence in the co-rotating frame: Keplerian angular speed ~ r^-1.5 winds the pattern into spirals.
    float omega = pow(r, -1.5) * uDiskSpeed;
    float phi = atan(p.z, p.x) - omega * uTime;
    float spiral = phi + log(r) * 2.5;
    vec3 q = vec3(cos(spiral) * 2.0, sin(spiral) * 2.0, r * 2.5);
    float density = smoothstep(0.25, 0.8, fbm(q) * 0.7 + fbm(q * 2.7 + 5.0) * 0.3);

    // Hot spots orbiting near the inner edge.
    float spots = 0.0;
    for (int i = 0; i < kHotSpots; ++i) {
        float rs = uInner * (1.2 + 0.3 * float(i));
        float angle = float(i) * 2.1 + pow(rs, -1.5) * uDiskSpeed * uTime;
        vec2 center = vec2(cos(angle), sin(angle)) * rs;
        spots += exp(-dot(p.xz - center, p.xz - center) / 0.35);
    }
    spots *= 0.6 + 1.6 * uSpotPulse;

    // Relativistic shifts: Doppler from the orbital velocity, gravitational redshift from depth in the well.
    float beta = min(sqrt(0.5 / max(r - kHorizon, 0.05)), 0.95);
    vec3 velocity = normalize(vec3(-p.z, 0.0, p.x)) * beta;  // counter-clockwise seen from +y
    vec3 toObserver = -normalize(rayDir);
    float gamma = inversesqrt(1.0 - beta * beta);
    float doppler = 1.0 / (gamma * (1.0 - dot(velocity, toObserver)));
    float gravity = sqrt(max(1.0 - kHorizon / r, 0.0));
    float shift = mix(1.0, doppler * gravity, uDoppler);

    float edge = smoothstep(uInner, uInner * 1.12, r) * smoothstep(uOuter, uOuter * 0.65, r);
    float alpha = clamp((density * 1.1 + spots) * edge, 0.0, 1.0) * uOpacity;
    float intensity = temperature * temperature * pow(shift, 3.0) * (0.4 + 1.6 * density + 3.0 * spots);
    vec3 color = blackbody(temperature * shift * uTemp) * intensity * uBrightness;
    return vec4(color * alpha, alpha);
}

vec3 background(vec3 d) {
    vec3 col = vec3(0.001, 0.001, 0.003);
    float band = exp(-pow(dot(d, uBandNormal) * 3.0, 2.0));
    float clouds = smoothstep(0.3, 0.8, fbm(d * 3.0));
    vec3 tint = mix(vec3(0.06, 0.035, 0.14), vec3(0.16, 0.07, 0.04), fbm(d * 7.0 + 11.0));
    col += (tint * clouds * band + vec3(0.03, 0.035, 0.05) * band * band) * uBackground;
    col += starSky(d, 1.0 / (uResolution.y * uFocal));
    return col;
}

void main() {
    vec3 pos = uCamPos;
    vec3 dir = cameraRay(gl_FragCoord.xy, uResolution);
    vec3 crossed = cross(pos, dir);
    float h2 = dot(crossed, crossed);  // squared angular momentum, conserved along the path
    float escape = length(uCamPos) + 20.0;

    vec3 col = vec3(0.0);
    float alpha = 0.0;
    bool captured = false;

    for (int i = 0; i < uMaxSteps; ++i) {
        float r = length(pos);
        if (r < kHorizon) {
            captured = true;
            break;
        }
        if (r > escape && dot(pos, dir) > 0.0) break;

        float dt = clamp((r - kHorizon) * 0.12, 0.012, 1.5) * uStepScale;
        vec3 previous = pos;
        dir += -1.5 * h2 * pos / pow(r, 5.0) * uLensing * dt;
        pos += dir * dt;

        // Disk crossings (a ray can cross several times: those are the secondary images and the photon ring).
        if (previous.y * pos.y < 0.0) {
            vec3 hit = mix(previous, pos, previous.y / (previous.y - pos.y));
            float rh = length(hit.xz);
            if (rh > uInner && rh < uOuter) {
                vec4 disk = diskSample(hit, dir);
                col += (1.0 - alpha) * disk.rgb;
                alpha += (1.0 - alpha) * disk.a;
                if (alpha > 0.99) break;
            }
        }

        // Polar jets: thin glowing cones along the spin axis.
        if (uJets > 0.0) {
            float height = abs(pos.y);
            float radius = length(pos.xz);
            float width = 0.2 + 0.1 * height;
            float jet = exp(-pow(radius / width, 2.0)) * exp(-height * 0.07) * smoothstep(1.5, 3.5, height);
            jet *= 0.7 + 0.3 * sin(height * 1.7 - uTime * 6.0);
            col += (1.0 - alpha) * vec3(0.4, 0.6, 1.0) * jet * uJets * dt * 0.8;
        }
    }

    if (!captured && alpha < 0.99) col += (1.0 - alpha) * background(normalize(dir));
    fragColor = vec4(col, 1.0);
}
