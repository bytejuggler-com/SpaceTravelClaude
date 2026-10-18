#version 460 core

// Soft round star stretched into a capsule along its motion. Drawn with additive blending.

in vec2 vLocal;
in vec3 vColor;
in float vStreak;
out vec4 fragColor;

void main() {
    float dx = max(abs(vLocal.x) - vStreak, 0.0);
    float d2 = dx * dx + vLocal.y * vLocal.y;
    fragColor = vec4(vColor * exp(-d2 * 1.5), 1.0);
}
