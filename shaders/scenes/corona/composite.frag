#version 460 core

#include "common/camera.glsl"
#include "common/noise.glsl"
#include "common/sky.glsl"

// Full-resolution pass: crisp star sky around the camera, with the reduced-resolution star layer
// (premultiplied: rgb light, a = disc coverage) upscaled on top.

in vec2 vUV;
out vec4 fragColor;

uniform vec2 uResolution;
uniform sampler2D uStar;

vec3 sky(vec3 rd) {
    vec3 col = vec3(0.002, 0.001, 0.005);
    col += vec3(0.035, 0.01, 0.03) * smoothstep(0.5, 0.9, fbm(rd * 2.5));
    col += vec3(0.0, 0.015, 0.035) * smoothstep(0.55, 0.95, fbm(rd * 4.0 + 7.0));
    col += starSky(rd, 1.0 / (uResolution.y * uFocal));
    return col;
}

void main() {
    vec3 rd = cameraRay(gl_FragCoord.xy, uResolution);
    vec4 star = texture(uStar, gl_FragCoord.xy / uResolution);
    fragColor = vec4(sky(rd) * (1.0 - star.a) + star.rgb, 1.0);
}
