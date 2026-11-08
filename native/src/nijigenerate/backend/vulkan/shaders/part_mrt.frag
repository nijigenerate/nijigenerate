// Copyright 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
#version 450
layout(constant_id = 0) const int blendEquation = 0;
layout(set=0,binding=0) uniform sampler2D albedoTex;
layout(set=0,binding=1) uniform sampler2D emissiveTex;
layout(set=0,binding=2) uniform sampler2D bumpTex;
layout(set=0,binding=3) uniform sampler2D backgroundTex;
layout(push_constant) uniform Push { vec4 tintOpacity; vec4 screenEmission; vec4 emission; } pc;
layout(location=0) in vec2 outUv;
layout(location=0) out vec4 color;
layout(location=1) out vec4 emissionColor;
layout(location=2) out vec4 bumpColor;
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
    if (albedo.a <= 0.0001) discard;
    vec3 screened = vec3(1.0) - ((vec3(1.0)-albedo.rgb) * (vec3(1.0)-pc.screenEmission.rgb*albedo.a));
    color = vec4(clamp(screened*pc.tintOpacity.rgb,0.0,1.0),albedo.a)*pc.tintOpacity.a;
    if (blendEquation == 1 || blendEquation == 2) {
        color.rgb *= color.a;
    } else if (blendEquation == 3) {
        vec4 background = texelFetch(backgroundTex,ivec2(gl_FragCoord.xy),0);
        vec3 blended = computeBlend(background.rgb,color.rgb);
        color = vec4(clamp(mix(background.rgb,blended,clamp(color.a,0,1)),0,1),
            clamp(color.a+background.a*(1-color.a),0,1));
    }
    vec4 emissive = texture(emissiveTex,outUv);
    float coverage = pc.emission.x > 0.5 ? 1.0 : albedo.a;
    vec3 screenedEmission = vec3(1.0)-((vec3(1.0)-emissive.rgb)*
        (vec3(1.0)-pc.screenEmission.rgb*albedo.a));
    vec3 outE = screenedEmission*pc.tintOpacity.rgb*max(pc.screenEmission.a,0.0)*coverage;
    emissionColor = vec4(clamp(outE,0.0,1.0),albedo.a)*pc.tintOpacity.a;
    bumpColor = texture(bumpTex,outUv)*albedo.a*pc.tintOpacity.a;
}
