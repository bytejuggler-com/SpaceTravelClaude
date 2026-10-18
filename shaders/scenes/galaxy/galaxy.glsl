// Density-wave spiral galaxy model shared by the galaxy particle shaders. Requires common/math.glsl and
// common/noise.glsl. The disc lies in the xz plane, radius ~110 units.
// Every particle moves on an ellipse whose orientation twists with its size; where neighboring ellipses crowd
// together, two spiral arms appear. Particles stream through the arms while the pattern turns slowly.
// Positions are pure functions of (id, time): scrubbable.

uniform float uTime;
uniform float uOrbitSpeed;  // angular speed scale (flat rotation curve: omega ~ 1 / radius)
uniform float uPattern;     // rotation of the whole spiral pattern (radians)
uniform float uTwist;       // ellipse orientation change per unit of radius

const float kDiskScale = 28.0;   // exponential disc scale length
const float kDiskEdge = 110.0;
const float kBulgeRadius = 14.0;

// Ellipse axis ratio: elongated orbits in the bar-like core, rounder further out.
float axisRatio(float a) {
    float k = mix(0.6, 0.82, smoothstep(0.0, 18.0, a));
    return mix(k, 0.95, smoothstep(60.0, 110.0, a));
}

vec3 orbitPosition(float a, float phase, float height) {
    float phi = phase + uOrbitSpeed / (a + 3.0) * uTime;
    vec2 e = vec2(a * cos(phi), a * axisRatio(a) * sin(phi));
    float theta = a * uTwist + uPattern;
    float c = cos(theta), s = sin(theta);
    return vec3(c * e.x - s * e.y, height, s * e.x + c * e.y);
}

// Exponential disc radius, truncated at the edge.
float diskRadius(float u) { return -kDiskScale * log(1.0 - u * (1.0 - exp(-kDiskEdge / kDiskScale))); }

float gaussian(float u1, float u2) { return sqrt(-2.0 * log(max(u1, 1e-6))) * cos(TAU * u2); }
