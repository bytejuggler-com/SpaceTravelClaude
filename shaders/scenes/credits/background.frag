#version 460 core

#include "common/camera.glsl"
#include "common/noise.glsl"
#include "common/sky.glsl"

// End credits backdrop: slowly turning starfield over a deep violet gradient with a faint nebula.

in vec2 vUV;
out vec4 fragColor;

uniform vec2 uResolution;
uniform float uTime;

void main() {
    vec3 rd = cameraRay(gl_FragCoord.xy, uResolution);
    float c = cos(uTime * 0.03), s = sin(uTime * 0.03);
    vec3 d = vec3(c * rd.x + s * rd.z, rd.y, -s * rd.x + c * rd.z);
    vec3 col = mix(vec3(0.02, 0.005, 0.04), vec3(0.002, 0.004, 0.015), gl_FragCoord.y / uResolution.y);
    col += vec3(0.06, 0.02, 0.08) * smoothstep(0.5, 0.9, fbm(d * 2.0));
    col += starSky(d, 1.0 / (uResolution.y * uFocal));
    fragColor = vec4(col, 1.0);
}
