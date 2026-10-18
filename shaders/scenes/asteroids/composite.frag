#version 460 core

// Scene color + light shafts + lens flare (ghosts along the star-to-center axis, anamorphic streak, halo),
// with the flare dimmed by how much of the star is hidden behind rocks.

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uFrame;
uniform sampler2D uShafts;
uniform sampler2D uDepth;
uniform vec2 uResolution;
uniform vec2 uLightUV;
uniform float uLightInFront;
uniform float uFlare;
uniform vec3 uStarColor;

const int kGhosts = 5;
const float kGhostPos[kGhosts] = float[](0.45, 0.85, 1.2, 1.55, 2.0);
const float kGhostSize[kGhosts] = float[](0.045, 0.025, 0.07, 0.018, 0.11);
const vec3 kGhostColor[kGhosts] = vec3[](vec3(0.25, 0.5, 1.0), vec3(1.0, 0.6, 0.2), vec3(0.3, 1.0, 0.5),
                                         vec3(1.0, 0.3, 0.6), vec3(0.4, 0.6, 1.0));

float starVisibility() {
    float visible = 0.0;
    vec2 texel = 4.0 / uResolution;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x) {
            vec2 p = uLightUV + vec2(x, y) * texel;
            if (all(greaterThanEqual(p, vec2(0.0))) && all(lessThanEqual(p, vec2(1.0))) && texture(uDepth, p).r >= 1.0)
                visible += 1.0;
        }
    return visible / 9.0;
}

void main() {
    vec2 uv = gl_FragCoord.xy / uResolution;
    vec3 col = texture(uFrame, uv).rgb + texture(uShafts, uv).rgb;

    if (uFlare > 0.0 && uLightInFront > 0.0) {
        float visibility = starVisibility();
        if (visibility > 0.0) {
            float aspect = uResolution.x / uResolution.y;
            vec2 toCenter = vec2(0.5) - uLightUV;
            vec3 flare = vec3(0.0);
            for (int i = 0; i < kGhosts; ++i) {
                vec2 center = uLightUV + toCenter * kGhostPos[i];
                float d = length((uv - center) * vec2(aspect, 1.0));
                flare += kGhostColor[i] * smoothstep(kGhostSize[i], kGhostSize[i] * 0.6, d) * 0.08;
            }
            vec2 fromLight = (uv - uLightUV) * vec2(aspect, 1.0);
            flare += uStarColor * exp(-abs(fromLight.y) * 260.0) * exp(-abs(fromLight.x) * 2.2) * 0.8;  // streak
            flare += vec3(0.3, 0.5, 1.0) * exp(-pow((length(fromLight) - 0.32) * 22.0, 2.0)) * 0.05;      // halo
            col += flare * visibility * uFlare;
        }
    }
    fragColor = vec4(col, 1.0);
}
