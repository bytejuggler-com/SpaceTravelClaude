// Hash and noise helpers shared by all scenes. Include with: #include "common/noise.glsl"
// Hashes are Dave Hoskins' "hash without sine" (stable across GPUs).

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec2 hash22(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx + p3.yz) * p3.zy);
}

float hash13(vec3 p3) {
    p3 = fract(p3 * 0.1031);
    p3 += dot(p3, p3.zyx + 31.32);
    return fract((p3.x + p3.y) * p3.z);
}

// Integer hash (PCG) for large ids, e.g. gl_VertexID of a million particles, where the float hashes lose precision.
uint pcgHash(uint v) {
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// Uniform random in [0, 1] for (id, channel).
float hashU(uint id, uint channel) { return float(pcgHash(id ^ pcgHash(channel))) / 4294967295.0; }

float valueNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1, 0)), u.x),
               mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), u.x), u.y);
}

float valueNoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);
    float n000 = hash13(i);
    float n100 = hash13(i + vec3(1, 0, 0));
    float n010 = hash13(i + vec3(0, 1, 0));
    float n110 = hash13(i + vec3(1, 1, 0));
    float n001 = hash13(i + vec3(0, 0, 1));
    float n101 = hash13(i + vec3(1, 0, 1));
    float n011 = hash13(i + vec3(0, 1, 1));
    float n111 = hash13(i + vec3(1, 1, 1));
    return mix(mix(mix(n000, n100, u.x), mix(n010, n110, u.x), u.y),
               mix(mix(n001, n101, u.x), mix(n011, n111, u.x), u.y), u.z);
}

// Fractal Brownian motion, roughly in [0, 1].
float fbm(vec2 p) {
    const mat2 rot = mat2(0.8, 0.6, -0.6, 0.8);
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 5; ++i) {
        value += amplitude * valueNoise(p);
        p = rot * p * 2.02 + 17.0;
        amplitude *= 0.5;
    }
    return value;
}

float fbm(vec3 p) {
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 5; ++i) {
        value += amplitude * valueNoise(p);
        p = p * 2.03 + vec3(17.0, 31.0, 7.0);
        amplitude *= 0.5;
    }
    return value;
}
