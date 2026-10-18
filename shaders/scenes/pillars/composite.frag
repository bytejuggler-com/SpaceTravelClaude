#version 460 core

#include "common/camera.glsl"
#include "common/noise.glsl"
#include "common/sky.glsl"

// Nebula crossing, full-resolution pass: crisp stars and the star cluster's glow behind the volume, dimmed by
// the volume's transmittance, with the (upscaled) volume light on top.

in vec2 vUV;
out vec4 fragColor;

uniform vec2 uResolution;
uniform sampler2D uVolume;
uniform vec3 uLightPos;
uniform vec3 uLightColor;

void main() {
    vec3 rd = cameraRay(gl_FragCoord.xy, uResolution);
    vec4 volume = texture(uVolume, gl_FragCoord.xy / uResolution);
    float transmittance = 1.0 - volume.a;

    vec3 sky = vec3(0.002, 0.001, 0.004) + starSky(rd, 1.0 / (uResolution.y * uFocal)) * 0.6;
    float angle = acos(clamp(dot(rd, normalize(uLightPos - uCamPos)), -1.0, 1.0));
    sky += uLightColor * (exp(-angle * 60.0) * 10.0 + exp(-angle * 12.0) * 0.8);

    fragColor = vec4(sky * transmittance + volume.rgb, 1.0);
}
