#version 460 core

// Upscales a reduced-resolution pass to the current target: color with bilinear filtering, depth copied
// (nearest) into gl_FragDepth so full-resolution geometry can be depth-tested against it.
// Draw with depth test ALWAYS and depth writes on.

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uColor;
uniform sampler2D uDepth;
uniform vec2 uResolution;  // of the current (full-resolution) target

void main() {
    vec2 uv = gl_FragCoord.xy / uResolution;
    fragColor = vec4(texture(uColor, uv).rgb, 1.0);
    gl_FragDepth = texture(uDepth, uv).r;
}
