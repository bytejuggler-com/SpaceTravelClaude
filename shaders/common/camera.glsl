// Pinhole camera for raymarched scenes; set from C++ with Camera::apply (src/scene/Camera.h).
// Include with: #include "common/camera.glsl"

uniform vec3 uCamPos;
uniform vec3 uCamRight;
uniform vec3 uCamUp;
uniform vec3 uCamForward;
uniform float uFocal;

// World-space ray direction through a pixel. Independent of render resolution (normalized by height),
// so half-resolution passes and full-resolution passes line up.
vec3 cameraRay(vec2 fragCoord, vec2 resolution) {
    vec2 uv = (fragCoord - 0.5 * resolution) / resolution.y;
    return normalize(uCamForward * uFocal + uCamRight * uv.x + uCamUp * uv.y);
}
