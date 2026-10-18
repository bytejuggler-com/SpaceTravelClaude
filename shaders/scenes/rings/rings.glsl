// Ring system shared by the ringed-giant passes. Requires common/noise.glsl.
// Planet of radius uPlanetRadius at the origin, rings in the y = 0 plane between uInner and uOuter.

uniform float uPlanetRadius;
uniform float uInner;
uniform float uOuter;

// Radial opacity: faint inner ring, dense middle ring, a wide gap and a narrow one, fine ringlets everywhere.
float ringOpacity(float r) {
    if (r < uInner || r > uOuter) return 0.0;
    float x = (r - uInner) / (uOuter - uInner);
    float base = mix(0.3, 0.95, smoothstep(0.1, 0.45, x)) * mix(1.0, 0.55, smoothstep(0.72, 0.95, x));
    float ringlets = (0.6 + 0.4 * sin(r * 0.9) * sin(r * 0.23 + 1.0)) * (0.65 + 0.35 * valueNoise(vec2(r * 2.0, 0.5)));
    float wideGap = smoothstep(2.0, 3.5, abs(r - (uInner + 0.69 * (uOuter - uInner))));
    float narrowGap = smoothstep(0.4, 0.9, abs(r - (uInner + 0.9 * (uOuter - uInner))));
    float edges = smoothstep(0.0, 0.03, x) * smoothstep(1.0, 0.96, x);
    return clamp(base * ringlets * wideGap * narrowGap * edges, 0.0, 0.97);
}

vec3 ringAlbedo(float r) {
    return mix(vec3(0.72, 0.65, 0.55), vec3(0.92, 0.87, 0.8), valueNoise(vec2(r * 0.7, 3.0)));
}

// Fraction of sunlight reaching point p past the planet (soft-edged shadow cylinder).
float planetShadow(vec3 p, vec3 L) {
    float along = dot(p, L);
    if (along > 0.0) return 1.0;  // on the sunward side
    float d = length(p - L * along);
    return smoothstep(uPlanetRadius - 2.0, uPlanetRadius + 2.0, d);
}

// Fraction of sunlight reaching point p past the rings (crossing the ring plane toward the sun).
float ringShadow(vec3 p, vec3 L) {
    if (abs(L.y) < 1e-4) return 1.0;
    float s = -p.y / L.y;
    if (s <= 0.0) return 1.0;
    return 1.0 - ringOpacity(length((p + L * s).xz)) * 0.9;
}
