// Shared interface for transition shaders in shaders/transitions/<name>.frag.
// A transition blends the outgoing scene (uFrom) into the incoming one (uTo); both are linear HDR.
// The timeline refers to transitions by file name, so adding a .frag here makes a new transition available.

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uFrom;
uniform sampler2D uTo;
uniform float uProgress;  // 0 -> 1, linear over the transition length
uniform float uTime;      // demo time (s)
uniform vec2 uResolution;

vec3 fromColor(vec2 uv) { return texture(uFrom, uv).rgb; }
vec3 toColor(vec2 uv) { return texture(uTo, uv).rgb; }
