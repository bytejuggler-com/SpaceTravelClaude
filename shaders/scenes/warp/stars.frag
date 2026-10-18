#version 460 core

// Capsule-shaped streak, brighter at the head. Drawn with additive blending.

in vec2 vLocal;
in vec3 vColor;
in float vLength;
out vec4 fragColor;

void main() {
    float dx = max(abs(vLocal.x) - vLength, 0.0);
    float d2 = dx * dx + vLocal.y * vLocal.y;
    float core = max(1.0 - d2, 0.0);
    float headward = vLocal.x / (vLength + 1.0) * 0.5 + 0.5;  // 0 at the tail, 1 at the head
    fragColor = vec4(vColor * core * core * mix(0.3, 1.0, headward), 1.0);
}
