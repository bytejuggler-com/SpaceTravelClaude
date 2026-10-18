#version 460 core

#include "common/color.glsl"
#include "common/font.glsl"
#include "common/noise.glsl"

// Amiga-style intro: copper bars behind a chunky chrome logo that bounces in, a rainbow sine scroller
// and its rippling reflection in "water" at the bottom. Linear HDR output.

in vec2 vUV;
out vec4 fragColor;

uniform float uTime;        // scene-local time (s)
uniform vec2 uResolution;
uniform float uKick;        // 1 on each kick, decaying
uniform float uBeatPulse;   // 1 on each beat, decaying
uniform int uLogoLength;    // characters in text line 0
uniform int uScrollLength;  // characters in text line 1
uniform float uScrollSpeed; // font pixels per second
uniform float uWobble;      // logo row wobble (font pixels)
uniform float uCopperSpeed;

const int kLogoLine = 0;
const int kScrollLine = 1;
const int kCopperBars = 7;

float easeOutBounce(float x) {
    const float n1 = 7.5625, d1 = 2.75;
    if (x < 1.0 / d1) return n1 * x * x;
    if (x < 2.0 / d1) { x -= 1.5 / d1; return n1 * x * x + 0.75; }
    if (x < 2.5 / d1) { x -= 2.25 / d1; return n1 * x * x + 0.9375; }
    x -= 2.625 / d1;
    return n1 * x * x + 0.984375;
}

vec3 background(vec2 frag) {
    float y = frag.y / uResolution.y;
    vec3 col = mix(vec3(0.0, 0.0, 0.015), vec3(0.02, 0.0, 0.05), y);
    // Two layers of tiny drifting stars (a teaser for the next scene).
    for (int layer = 0; layer < 2; ++layer) {
        float speed = layer == 0 ? 12.0 : 30.0;
        vec2 cell = floor((frag + vec2(uTime * speed, 0.0)) / 3.0);
        float h = hash12(cell + float(layer) * 71.0);
        if (h > 0.9975) col += vec3(0.6, 0.7, 1.0) * (layer == 0 ? 0.6 : 1.5);
    }
    return col;
}

// Raster bars swinging around the middle of the screen; nearer bars cover farther ones.
vec4 copperBars(vec2 frag) {
    float y = frag.y / uResolution.y;
    vec4 result = vec4(0.0);
    float nearest = -2.0;
    for (int i = 0; i < kCopperBars; ++i) {
        float phase = uTime * uCopperSpeed + float(i) * 0.45;
        float center = 0.6 + 0.24 * sin(phase);
        float depth = cos(phase);  // +1 = in front
        float d = abs(y - center) / 0.032;
        if (d < 1.0 && depth > nearest) {
            nearest = depth;
            vec3 base = palette(float(i) / float(kCopperBars) + uTime * 0.05,
                                vec3(0.5), vec3(0.5), vec3(1.0), vec3(0.0, 0.33, 0.67));
            float shade = 1.0 - d * d;  // rounded tube profile
            vec3 col = base * (0.25 + 1.4 * shade) * (0.55 + 0.45 * depth);
            col += vec3(1.0) * pow(shade, 16.0) * 0.9;  // specular line
            result = vec4(col * (1.0 + 0.8 * uKick), 1.0);
        }
    }
    return result;
}

struct LogoSample {
    float coverage;
    float shadow;
    vec3 color;
};

LogoSample logo(vec2 frag) {
    LogoSample s = LogoSample(0.0, 0.0, vec3(0.0));
    float pix = max(1.0, floor(uResolution.x * 0.84 / float(uLogoLength * 8)));
    vec2 size = vec2(float(uLogoLength * 8), 8.0) * pix;
    float drop = (1.0 - easeOutBounce(clamp(uTime / 1.6, 0.0, 1.0))) * uResolution.y * 0.7;
    vec2 origin = vec2((uResolution.x - size.x) * 0.5, uResolution.y * 0.64 - size.y * 0.5 + drop);

    vec2 local = (frag - origin) / pix;  // font pixels, y up
    float rowFromTop = 7.0 - floor(local.y);
    local.x += sin(rowFromTop * 0.7 + uTime * 2.5) * uWobble * (0.4 + uBeatPulse);

    ivec2 p = ivec2(floor(local.x), 7 - int(floor(local.y)));
    vec2 shadowLocal = local + vec2(-0.5, 0.5);  // shadow falls down-right
    s.shadow = textPixel(ivec2(floor(shadowLocal.x), 7 - int(floor(shadowLocal.y))), kLogoLine, uLogoLength);
    s.coverage = textPixel(p, kLogoLine, uLogoLength);
    if (s.coverage < 0.5) return s;

    // Chrome: sky-blue to white down to the "horizon" in the middle, then brown to gold.
    float t = float(p.y) / 7.0;
    vec3 chrome = t < 0.5 ? mix(vec3(0.15, 0.4, 1.0), vec3(1.0), t * 2.0)
                          : mix(vec3(0.45, 0.18, 0.03), vec3(1.0, 0.78, 0.3), (t - 0.5) * 2.0);

    // Per-block bevel and mortar lines for the chunky look.
    vec2 f = fract(local);
    float tile = step(0.07, f.x) * step(0.07, f.y);
    float light = smoothstep(0.75, 0.95, f.y) + smoothstep(0.25, 0.05, f.x);
    float dark = smoothstep(0.25, 0.05, f.y) + smoothstep(0.75, 0.95, f.x);

    // Diagonal shine sweeping across.
    float band = fract((local.x + (8.0 - local.y)) / 70.0 - uTime * 0.3);
    float shine = exp(-pow((band - 0.5) * 14.0, 2.0));

    s.color = (chrome * (1.0 + 0.6 * light - 0.5 * dark) * (1.6 + 1.2 * uBeatPulse) + vec3(3.0) * shine) * tile;
    return s;
}

vec4 scroller(vec2 frag, float pix, float baseY) {
    float tx = (frag.x - uResolution.x) / pix + uTime * uScrollSpeed;  // text column; 0 enters at the right edge
    if (tx < 0.0) return vec4(0.0);
    float wave = sin(tx * 0.04 + uTime * 2.0) * 2.5 + sin(tx * 0.013 - uTime * 1.3) * 1.5;
    float ty = (frag.y - baseY) / pix - wave;  // 0..8 bottom-up inside the text
    if (ty < 0.0 || ty >= 8.0) return vec4(0.0);

    float x = mod(tx, float(uScrollLength * 8 + 48));  // repeat with a gap
    ivec2 p = ivec2(floor(x), 7 - int(floor(ty)));
    float on = textPixel(p, kScrollLine, uScrollLength);

    vec2 f = fract(vec2(x, ty));
    float tile = step(0.1, f.x) * step(0.1, f.y);
    vec3 c = palette(tx * 0.004 + uTime * 0.15, vec3(0.6), vec3(0.4), vec3(1.0), vec3(0.0, 0.33, 0.67));
    c *= mix(0.7, 1.3, f.y) * 2.2 * tile;
    return vec4(c, on);
}

void main() {
    vec2 frag = gl_FragCoord.xy;
    float scrollPix = max(1.0, floor(uResolution.y / 90.0));
    float scrollBase = uResolution.y * 0.13;
    float waterY = scrollBase - 3.0 * scrollPix;

    vec3 col;
    if (frag.y >= waterY) {
        col = background(frag);

        vec4 bars = copperBars(frag);
        col = mix(col, bars.rgb, bars.a);

        LogoSample l = logo(frag);
        col *= 1.0 - 0.75 * l.shadow;
        col = mix(col, l.color, l.coverage);

        vec4 text = scroller(frag, scrollPix, scrollBase);
        col = mix(col, text.rgb, text.a);
    } else {
        // Water: dark gradient with a rippling, tinted reflection of the scroller.
        float depth = (waterY - frag.y) / waterY;
        col = mix(vec3(0.0, 0.02, 0.06), vec3(0.0, 0.0, 0.01), depth);
        vec2 mirrored = vec2(frag.x + sin(frag.y * 0.15 + uTime * 5.0) * scrollPix * 0.4, 2.0 * waterY - frag.y);
        vec4 text = scroller(mirrored, scrollPix, scrollBase);
        col += text.rgb * text.a * vec3(0.35, 0.5, 0.9) * 0.4 * (1.0 - depth);
        col += vec3(0.3, 0.5, 1.0) * exp(-(waterY - frag.y) * 0.5) * 0.6;  // bright water line
    }

    fragColor = vec4(col, 1.0);
}
