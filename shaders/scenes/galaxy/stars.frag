#version 460 core

// Soft round point sprite, added on top of everything.

in vec3 vColor;
out vec4 fragColor;

void main() {
    vec2 d = gl_PointCoord * 2.0 - 1.0;
    fragColor = vec4(vColor * exp(-dot(d, d) * 3.0), 1.0);
}
