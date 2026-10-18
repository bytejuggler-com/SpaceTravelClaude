#version 460 core

#include "common/camera.glsl"
#include "common/noise.glsl"
#include "common/sky.glsl"

// Backdrop for the asteroid field: stars, a faint dust glow, and the small bright star that lights the scene.
// Drawn first without depth writes, so the depth buffer keeps 1.0 wherever the sky shows (used by the shafts).

in vec2 vUV;
out vec4 fragColor;

uniform vec2 uResolution;
uniform vec3 uLightDir;
uniform vec3 uStarColor;
uniform float uStarSize;  // angular radius of the star disc (radians)

void main() {
    vec3 rd = cameraRay(gl_FragCoord.xy, uResolution);
    float pixelAngle = 1.0 / (uResolution.y * uFocal);

    vec3 col = vec3(0.0015, 0.0012, 0.003);
    col += vec3(0.03, 0.02, 0.015) * smoothstep(0.55, 0.9, fbm(rd * 2.2));
    col += starSky(rd, pixelAngle) * 0.8;

    float angle = acos(clamp(dot(rd, uLightDir), -1.0, 1.0));
    float disc = smoothstep(uStarSize + pixelAngle, uStarSize - pixelAngle, angle);
    col += uStarColor * (disc * 60.0 + exp(-angle * 45.0) * 2.0 + exp(-angle * 7.0) * 0.12);
    fragColor = vec4(col, 1.0);
}
