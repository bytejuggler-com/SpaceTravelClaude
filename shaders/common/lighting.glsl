// Directional light with cascaded soft shadows, plus small shading helpers for mesh scenes.
// Requires common/math.glsl and common/noise.glsl. Include with: #include "common/lighting.glsl"
// Uniforms are set from C++ (see AsteroidsScene for an example of the full setup).

uniform vec3 uLightDir;        // unit vector toward the light
uniform vec3 uLightColor;      // HDR intensity
uniform sampler2DArrayShadow uShadowMap;
uniform mat4 uLightMatrix[2];  // world -> light clip space, per cascade
uniform vec4 uCascade[2];      // xyz center, w radius of each cascade's bounding sphere
uniform float uShadowTexel;    // 1 / shadow map size
uniform float uShadowSoftness; // filter radius in texels

const int kCascades = 2;
const vec2 kPoisson[12] = vec2[](
    vec2(-0.326, -0.406), vec2(-0.840, -0.074), vec2(-0.696, 0.457), vec2(-0.203, 0.621),
    vec2(0.962, -0.195), vec2(0.473, -0.480), vec2(0.519, 0.767), vec2(0.185, -0.893),
    vec2(0.507, 0.064), vec2(0.896, 0.412), vec2(-0.322, -0.933), vec2(-0.792, -0.598));

float sampleCascade(int cascade, vec3 worldPos, float rotation) {
    vec4 clip = uLightMatrix[cascade] * vec4(worldPos, 1.0);
    vec3 coord = clip.xyz / clip.w * 0.5 + 0.5;
    if (any(lessThan(coord.xy, vec2(0.0))) || any(greaterThan(coord.xy, vec2(1.0))) || coord.z > 1.0) return 1.0;

    float c = cos(rotation), s = sin(rotation);
    mat2 spin = mat2(c, s, -s, c);  // per-pixel rotated Poisson disc: soft edges without banding
    float radius = uShadowSoftness * uShadowTexel;
    float lit = 0.0;
    for (int i = 0; i < 12; ++i)
        lit += texture(uShadowMap, vec4(coord.xy + spin * kPoisson[i] * radius, float(cascade), coord.z - 0.0005));
    return lit / 12.0;
}

// 1 = fully lit, 0 = in shadow. `normal` is the geometric normal, used to push the lookup off the surface.
float shadowFactor(vec3 worldPos, vec3 normal) {
    float rotation = hash12(gl_FragCoord.xy) * TAU;
    for (int i = 0; i < kCascades; ++i) {
        if (distance(worldPos, uCascade[i].xyz) < uCascade[i].w * 0.92 || i == kCascades - 1) {
            float texelWorld = 2.0 * uCascade[i].w * uShadowTexel;  // world size of one shadow texel
            return sampleCascade(i, worldPos + normal * texelWorld * 1.5, rotation);
        }
    }
    return 1.0;
}

// Bump mapping without tangents (Mikkelsen): perturb N by the screen-space gradient of a height field.
vec3 perturbNormal(vec3 normal, vec3 worldPos, float height, float strength) {
    vec3 dpdx = dFdx(worldPos);
    vec3 dpdy = dFdy(worldPos);
    float dhdx = dFdx(height);
    float dhdy = dFdy(height);
    vec3 r1 = cross(dpdy, normal);
    vec3 r2 = cross(normal, dpdx);
    float det = dot(dpdx, r1);
    vec3 gradient = sign(det) * (dhdx * r1 + dhdy * r2);
    return normalize(abs(det) * normal - strength * gradient);
}

float fresnelSchlick(float cosTheta, float f0) { return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0); }

// Normalized Blinn-Phong specular from a roughness value (0 = mirror, 1 = matte).
float specular(vec3 N, vec3 V, vec3 L, float roughness) {
    vec3 H = normalize(L + V);
    float power = 2.0 / max(roughness * roughness, 1e-3);
    return pow(max(dot(N, H), 0.0), power) * (power + 8.0) / 25.0;
}
