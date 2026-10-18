#version 460 core

// Instanced mesh vertex shader. Per-instance data comes from a storage buffer at binding 0; draws pick
// their slice with baseInstance (gl_BaseInstance), so one buffer serves every draw of a frame.

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in float aMaterial;
layout(location = 3) in float aOcclusion;

struct Instance {
    mat4 model;  // rotation * uniform scale + translation
    vec4 color;  // per-instance tint / parameters, meaning up to the fragment shader
};
layout(std430, binding = 0) readonly buffer InstanceBuffer { Instance instances[]; };

uniform mat4 uViewProj;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vObjectPos;
out vec4 vColor;
flat out float vMaterial;
out float vOcclusion;

void main() {
    Instance instance = instances[gl_BaseInstance + gl_InstanceID];
    vec4 world = instance.model * vec4(aPosition, 1.0);
    vWorldPos = world.xyz;
    vNormal = normalize(mat3(instance.model) * aNormal);
    vObjectPos = aPosition;
    vColor = instance.color;
    vMaterial = aMaterial;
    vOcclusion = aOcclusion;
    gl_Position = uViewProj * world;
}
