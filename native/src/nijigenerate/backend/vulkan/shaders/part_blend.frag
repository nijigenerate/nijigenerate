#version 450
layout(set = 0, binding = 0) uniform sampler2D albedoTex;
layout(set = 0, binding = 1) uniform sampler2D emissiveTex;
layout(set = 0, binding = 2) uniform sampler2D bumpTex;
layout(set = 0, binding = 3) uniform sampler2D backgroundTex;
layout(push_constant) uniform Push {
    vec4 tintOpacity;     // rgb=tint, a=opacity
    vec4 screenEmission;  // rgb=screen, a=emission
    vec4 emission; // y: portable blend equation
} pc;
layout(location = 0) in vec2 outUv;
layout(location = 0) out vec4 outColor;
const int BLEND_NORMAL      = 0;
const int BLEND_MULTIPLY    = 1;
const int BLEND_SCREEN      = 2;
const int BLEND_OVERLAY     = 3;
const int BLEND_DARKEN      = 4;
const int BLEND_LIGHTEN     = 5;
const int BLEND_COLORDODGE  = 6;
const int BLEND_LINEARDODGE = 7; // unused here, handled by legacy pipeline
const int BLEND_ADDGLOW     = 8; // unused here, handled by legacy pipeline
const int BLEND_COLORBURN   = 9;
const int BLEND_HARDLIGHT   = 10;
const int BLEND_SOFTLIGHT   = 11;
const int BLEND_DIFFERENCE  = 12;
const int BLEND_EXCLUSION   = 13;

vec3 blendMultiply(vec3 bg, vec3 fg) {
    return bg * fg;
}

vec3 blendScreen(vec3 bg, vec3 fg) {
    return bg + fg - bg * fg;
}

vec3 blendOverlay(vec3 bg, vec3 fg) {
    vec3 dark = 2.0 * bg * fg;
    vec3 light = 1.0 - 2.0 * (1.0 - bg) * (1.0 - fg);
    bvec3 mask = lessThan(bg, vec3(0.5));
    return vec3(
        mask.x ? dark.x : light.x,
        mask.y ? dark.y : light.y,
        mask.z ? dark.z : light.z
    );
}

vec3 blendDarken(vec3 bg, vec3 fg) {
    return min(bg, fg);
}

vec3 blendLighten(vec3 bg, vec3 fg) {
    return max(bg, fg);
}

vec3 blendColorDodge(vec3 bg, vec3 fg) {
    vec3 denom = max(vec3(1e-5), 1.0 - fg);
    return clamp(bg / denom, 0.0, 1.0);
}

vec3 blendColorBurn(vec3 bg, vec3 fg) {
    vec3 denom = max(vec3(1e-5), fg);
    return 1.0 - clamp((1.0 - bg) / denom, 0.0, 1.0);
}

vec3 blendHardLight(vec3 bg, vec3 fg) {
    vec3 dark = 2.0 * bg * fg;
    vec3 light = 1.0 - 2.0 * (1.0 - bg) * (1.0 - fg);
    bvec3 mask = lessThan(fg, vec3(0.5));
    return vec3(
        mask.x ? dark.x : light.x,
        mask.y ? dark.y : light.y,
        mask.z ? dark.z : light.z
    );
}

vec3 blendSoftLight(vec3 bg, vec3 fg) {
    vec3 sqrtBg = sqrt(clamp(bg, 0.0, 1.0));
    vec3 dark = bg - (1.0 - 2.0 * fg) * bg * (1.0 - bg);
    vec3 light = bg + (2.0 * fg - 1.0) * (sqrtBg - bg);
    bvec3 mask = lessThan(fg, vec3(0.5));
    return vec3(
        mask.x ? dark.x : light.x,
        mask.y ? dark.y : light.y,
        mask.z ? dark.z : light.z
    );
}

vec3 blendDifference(vec3 bg, vec3 fg) {
    return abs(bg - fg);
}

vec3 blendExclusion(vec3 bg, vec3 fg) {
    return bg + fg - 2.0 * bg * fg;
}

vec3 computeBlend(vec3 bg, vec3 fg) {
    if (int(pc.emission.y) == BLEND_MULTIPLY) return blendMultiply(bg, fg);
    if (int(pc.emission.y) == BLEND_SCREEN) return blendScreen(bg, fg);
    if (int(pc.emission.y) == BLEND_OVERLAY) return blendOverlay(bg, fg);
    if (int(pc.emission.y) == BLEND_DARKEN) return blendDarken(bg, fg);
    if (int(pc.emission.y) == BLEND_LIGHTEN) return blendLighten(bg, fg);
    if (int(pc.emission.y) == BLEND_COLORDODGE) return blendColorDodge(bg, fg);
    if (int(pc.emission.y) == BLEND_COLORBURN) return blendColorBurn(bg, fg);
    if (int(pc.emission.y) == BLEND_HARDLIGHT) return blendHardLight(bg, fg);
    if (int(pc.emission.y) == BLEND_SOFTLIGHT) return blendSoftLight(bg, fg);
    if (int(pc.emission.y) == BLEND_DIFFERENCE) return blendDifference(bg, fg);
    if (int(pc.emission.y) == BLEND_EXCLUSION) return blendExclusion(bg, fg);
    return fg;
}


void main() {
    vec4 albedo = texture(albedoTex, outUv);
    if (albedo.a <= 0.0001) {
        discard;
    }
    vec3 screen = vec3(1.0) - ((vec3(1.0)-albedo.rgb) * (vec3(1.0)-pc.screenEmission.rgb*albedo.a));
    // Emission is rendered separately and added after ambient-light modulation.
    vec4 foreground=vec4(clamp(screen*pc.tintOpacity.rgb,0.0,1.0),albedo.a)*pc.tintOpacity.a;
    vec4 background=texelFetch(backgroundTex,ivec2(gl_FragCoord.xy),0);
    vec3 blended=computeBlend(background.rgb,foreground.rgb);
    outColor=vec4(clamp(mix(background.rgb,blended,clamp(foreground.a,0,1)),0,1),
        clamp(foreground.a+background.a*(1-foreground.a),0,1));
}
