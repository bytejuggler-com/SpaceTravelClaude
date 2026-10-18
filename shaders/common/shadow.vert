#version 460 core

// Depth-only pass into a shadow-map cascade; same instance layout as common/mesh.vert.

layout(location = 0) in vec3 aPosition;

struct Instance {
    mat4 model;
    vec4 color;
};
layout(std430, binding = 0) readonly buffer InstanceBuffer { Instance instances[]; };

uniform mat4 uLightViewProj;

void main() {
    gl_Position = uLightViewProj * instances[gl_BaseInstance + gl_InstanceID].model * vec4(aPosition, 1.0);
}
