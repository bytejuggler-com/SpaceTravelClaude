// Small math helpers. Include with: #include "common/math.glsl"

const float PI = 3.14159265;
const float TAU = 6.28318531;

mat2 rotate2d(float angle) {
    float c = cos(angle), s = sin(angle);
    return mat2(c, s, -s, c);
}

// Quad corner for vertex i (0..5) of two triangles, in [-1, 1]^2.
vec2 quadCorner(int i) {
    const vec2 corners[6] = vec2[](vec2(-1, -1), vec2(1, -1), vec2(1, 1), vec2(-1, -1), vec2(1, 1), vec2(-1, 1));
    return corners[i];
}
