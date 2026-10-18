// Canyon terrain shared by the terrain pass and the fighter shading. Requires a sampler3D uNoise (the shared
// noise volume; one fixed slice is used as 2D noise). Coordinates: x across, z along the flight, y up.
// Keep canyonCenter / canyonHalfWidth in sync with CanyonScene.cpp (the flight path follows them).

uniform sampler3D uNoise;

float terrainNoise(vec2 p) { return texture(uNoise, vec3(p, 0.37)).r; }

float canyonCenter(float z) { return 40.0 * sin(z * 0.006) + 18.0 * sin(z * 0.017 + 1.0); }
float canyonHalfWidth(float z) { return 28.0 + 8.0 * sin(z * 0.011 + 2.0); }

// Stepped cliff profile: flat ledges joined by steep rises.
float terrace(float x) {
    float f = x * 5.0;
    return (floor(f) + smoothstep(0.55, 1.0, fract(f))) / 5.0;
}

float terrainHeight(vec2 p) {
    vec4 coarse = texture(uNoise, vec3(p * 0.0021, 0.37));
    vec4 medium = texture(uNoise, vec3(p * 0.013 + 0.3, 0.53));
    float mesa = 70.0 + (coarse.r - 0.5) * 40.0 + (medium.g - 0.5) * 12.0;
    float d = abs(p.x - canyonCenter(p.y)) + (medium.r - 0.5) * 10.0;  // eroded walls
    float w = canyonHalfWidth(p.y);
    float t = clamp((d - 0.35 * w) / (0.9 * w), 0.0, 1.0);
    float ground = (coarse.g - 0.5) * 2.5;
    return mix(ground, mesa, terrace(t));
}

// Soft sun visibility over the heightfield (penumbra from the closest miss).
float terrainShadow(vec3 p, vec3 L) {
    float visibility = 1.0;
    float t = 1.0;
    for (int i = 0; i < 24; ++i) {
        vec3 q = p + L * t;
        if (q.y > 140.0) break;  // above every mesa
        float h = q.y - terrainHeight(q.xz);
        visibility = min(visibility, 10.0 * h / t);
        if (visibility < 0.01) break;
        t += clamp(h * 0.5, 1.0, 25.0);
    }
    return clamp(visibility, 0.0, 1.0);
}
