// Color helpers. Include with: #include "common/color.glsl"

float luminance(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

// Narkowicz ACES filmic fit.
vec3 acesFitted(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

vec3 linearToSrgb(vec3 c) {
    c = clamp(c, 0.0, 1.0);
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(0.0031308, c));
}

// Smooth cosine palette (Inigo Quilez): a + b * cos(2pi * (c * t + d)).
vec3 palette(float t, vec3 a, vec3 b, vec3 c, vec3 d) {
    return a + b * cos(6.28318 * (c * t + d));
}
