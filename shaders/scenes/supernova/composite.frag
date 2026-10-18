#version 460 core

#include "common/camera.glsl"
#include "common/noise.glsl"
#include "common/sky.glsl"

// Supernova, full-resolution pass: stars behind the volume (dimmed by its transmittance), the upscaled volume
// light, and the blinding flash.

in vec2 vUV;
out vec4 fragColor;

uniform vec2 uResolution;
uniform sampler2D uVolume;
uniform float uFlash;

void main() {
    vec3 rd = cameraRay(gl_FragCoord.xy, uResolution);
    vec4 volume = texture(uVolume, gl_FragCoord.xy / uResolution);
    vec3 sky = vec3(0.0015, 0.001, 0.003) + starSky(rd, 1.0 / (uResolution.y * uFocal)) * 0.7;

    // Flash: everything washes out, brightest toward the explosion.
    float toCenter = max(dot(rd, normalize(-uCamPos)), 0.0);
    vec3 flash = vec3(1.0, 0.95, 0.9) * uFlash * (0.6 + 2.0 * pow(toCenter, 40.0));

    fragColor = vec4(sky * (1.0 - volume.a) + volume.rgb + flash, 1.0);
}
