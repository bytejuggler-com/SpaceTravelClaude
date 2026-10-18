#version 460 core

// Gaussian cloud puff. Gas is added (blend ONE, ONE); dust is multiplied in (blend ZERO, ONE_MINUS_SRC_COLOR),
// so its color is the fraction of light it absorbs.

in vec2 vLocal;
in vec3 vColor;
out vec4 fragColor;

void main() {
    fragColor = vec4(vColor * exp(-dot(vLocal, vLocal) * 3.0), 1.0);
}
