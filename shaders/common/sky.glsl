// Stars on the celestial sphere, for scenes with a 3D camera. Requires common/noise.glsl.
// Include with: #include "common/sky.glsl"

// One layer of stars: 3D cells around the unit sphere scaled by `scale`. A star belongs to the cell that
// contains its (sphere-projected) center, and the 2x2x2 cells nearest the sample are searched, so stars are
// never clipped at cell borders. `pixelAngle` is the angular size of a pixel (1 / (resolution.y * focal)).
vec3 starLayer(vec3 rd, float scale, float threshold, float gain, float seed, float pixelAngle) {
    vec3 q = rd * scale;
    vec3 base = floor(q - 0.5);
    vec3 col = vec3(0.0);
    for (int i = 0; i < 8; ++i) {
        vec3 cell = base + vec3(i & 1, (i >> 1) & 1, (i >> 2) & 1);
        float h = hash13(cell + seed);
        if (h < threshold) continue;
        vec3 jitter = vec3(hash13(cell + seed + 1.3), hash13(cell + seed + 2.7), hash13(cell + seed + 5.1));
        vec3 center = normalize(cell + jitter) * scale;
        if (floor(center) != cell) continue;
        float pixels = length(q - center) / scale / pixelAngle;
        float brightness = 0.3 + (h - threshold) / (1.0 - threshold);
        vec3 tint = mix(vec3(0.7, 0.8, 1.0), vec3(1.0, 0.85, 0.7), hash13(cell + seed + 9.0));
        col += tint * gain * brightness * exp(-pixels * pixels * 0.7);
    }
    return col;
}

// Two layers: sparse bright stars and a dense faint field.
vec3 starSky(vec3 rd, float pixelAngle) {
    return starLayer(rd, 90.0, 0.9, 2.5, 0.0, pixelAngle) + starLayer(rd, 220.0, 0.9, 0.8, 41.0, pixelAngle);
}
