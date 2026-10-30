#pragma once
// Embedded from the current independent nijilive shader sources.
#include <string_view>
namespace nijigenerate::backend::runtime_gl::geometry_shaders {
inline constexpr std::string_view basicVertex = R"NGSHADER(/*
    Copyright © 2020, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
#version 330
uniform mat4 mvp;
uniform vec2 offset;

layout(location = 0) in float vertX;
layout(location = 1) in float vertY;
layout(location = 2) in float uvX;
layout(location = 3) in float uvY;
layout(location = 4) in float deformX;
layout(location = 5) in float deformY;

out vec2 texUVs;

void main() {
    vec2 verts = vec2(vertX, vertY);
    vec2 deform = vec2(deformX, deformY);
    vec2 uvs = vec2(uvX, uvY);
    gl_Position = mvp * vec4(verts.x - offset.x + deform.x, verts.y - offset.y + deform.y, 0, 1);
    texUVs = uvs;
}
)NGSHADER";
inline constexpr std::string_view basicFragment = R"NGSHADER(/*
    Copyright © 2020, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
#version 330
in vec2 texUVs;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outEmissive;
layout(location = 2) out vec4 outBump;

uniform sampler2D albedo;
uniform sampler2D emissive;
uniform sampler2D bumpmap;

uniform float opacity;
uniform vec3 multColor;
uniform vec3 screenColor;
uniform float emissionStrength = 1;

vec4 screen(vec3 tcol, float a) {
    return vec4(vec3(1.0) - ((vec3(1.0)-tcol) * (vec3(1.0)-(screenColor*a))), a);
}

void main() {
    // Sample texture
    vec4 texColor = texture(albedo, texUVs);
    vec4 emiColor = texture(emissive, texUVs);
    vec4 bmpColor = texture(bumpmap, texUVs);

    vec4 mult = vec4(multColor.xyz, 1);

    // Out color math
    vec4 albedoOut = screen(texColor.xyz, texColor.a) * mult;
    vec4 emissionOut = screen(emiColor.xyz, texColor.a) * mult * emissionStrength;
    
    // Albedo
    outAlbedo = albedoOut * opacity;

    // Emissive
    outEmissive = emissionOut * outAlbedo.a;

    // Bumpmap
    outBump = bmpColor * outAlbedo.a;
})NGSHADER";
inline constexpr std::string_view stage1 = R"NGSHADER(/*
    Copyright © 2020, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen

    ADVANCED BLENDING - STAGE 1
*/
#version 330

// Advanced blendig mode enable
#ifdef GL_KHR_blend_equation_advanced 
#extension GL_KHR_blend_equation_advanced : enable
#endif

#ifdef GL_ARB_sample_shading
#extension GL_ARB_sample_shading : enable
#endif

in vec2 texUVs;

// Handle layout qualifiers for advanced blending specially
#ifdef GL_KHR_blend_equation_advanced 
    layout(blend_support_all_equations) out;
    layout(location = 0) out vec4 outAlbedo;
#else
    layout(location = 0) out vec4 outAlbedo;
#endif

uniform sampler2D albedo;

uniform float opacity;
uniform vec3 multColor;
uniform vec3 screenColor;

void main() {
    // Sample texture
    vec4 texColor = texture(albedo, texUVs);

    // Screen color math
    vec3 screenOut = vec3(1.0) - ((vec3(1.0)-(texColor.xyz)) * (vec3(1.0)-(screenColor*texColor.a)));
    
    // Multiply color math + opacity application.
    outAlbedo = vec4(screenOut.xyz, texColor.a) * vec4(multColor.xyz, 1) * opacity;
})NGSHADER";
inline constexpr std::string_view stage2 = R"NGSHADER(/*
    Copyright © 2020, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen

    ADVANCED BLENDING - STAGE 2
*/
#version 330
in vec2 texUVs;

layout(location = 1) out vec4 outEmissive;
layout(location = 2) out vec4 outBump;

uniform sampler2D albedo;
uniform sampler2D emissive;
uniform sampler2D bumpmap;

uniform float opacity;
uniform vec3 multColor;
uniform vec3 screenColor;
uniform float emissionStrength = 1;

vec4 screen(vec3 tcol, float a) {
    return vec4(vec3(1.0) - ((vec3(1.0)-tcol) * (vec3(1.0)-(screenColor*a))), a);
}

void main() {
    // Sample texture
    vec4 texColor = texture(albedo, texUVs);
    vec4 emiColor = texture(emissive, texUVs);
    vec4 bmpColor = texture(bumpmap, texUVs);

    vec4 mult = vec4(multColor.xyz, 1);

    // Out color math
    vec4 emissionOut = screen(emiColor.xyz, texColor.a) * mult * emissionStrength;

    // Emissive
    outEmissive = emissionOut * texColor.a;

    // Bumpmap
    outBump = bmpColor * texColor.a;
})NGSHADER";
inline constexpr std::string_view partMask = R"NGSHADER(/*
    Copyright © 2020, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
#version 330
in vec2 texUVs;
out vec4 outColor;

uniform sampler2D tex;
uniform float threshold;

void main() {
    vec4 color = texture(tex, texUVs);
    if (color.a <= threshold) discard;
    outColor = vec4(1, 1, 1, 1);
})NGSHADER";
inline constexpr std::string_view maskVertex = R"NGSHADER(/*
    Copyright © 2020, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
#version 330
uniform mat4 mvp;
uniform vec2 offset;
layout(location = 0) in float vertX;
layout(location = 1) in float vertY;
layout(location = 2) in float deformX;
layout(location = 3) in float deformY;

out vec2 texUVs;

void main() {
    vec2 verts = vec2(vertX, vertY);
    vec2 deform = vec2(deformX, deformY);
    gl_Position = mvp * vec4(verts.x - offset.x + deform.x, verts.y - offset.y + deform.y, 0, 1);
}
)NGSHADER";
inline constexpr std::string_view maskFragment = R"NGSHADER(/*
    Copyright © 2020, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
#version 330
out vec4 outColor;

void main() {
    outColor = vec4(0, 0, 0, 1);
})NGSHADER";
inline constexpr std::string_view advancedBlend = R"NGSHADER(/*
    Advanced blend mode fallback shader for platforms without GL_KHR_blend_equation_advanced
*/
#version 330

in vec2 texUVs;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outEmissive;
layout(location = 2) out vec4 outBump;

// Background Textures
uniform sampler2D bg_albedo;
uniform sampler2D bg_emissive;
uniform sampler2D bg_bump;

// Foreground Textures
uniform sampler2D fg_albedo;
uniform sampler2D fg_emissive;
uniform sampler2D fg_bump;

uniform int blend_mode;

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
    if (blend_mode == BLEND_MULTIPLY) return blendMultiply(bg, fg);
    if (blend_mode == BLEND_SCREEN) return blendScreen(bg, fg);
    if (blend_mode == BLEND_OVERLAY) return blendOverlay(bg, fg);
    if (blend_mode == BLEND_DARKEN) return blendDarken(bg, fg);
    if (blend_mode == BLEND_LIGHTEN) return blendLighten(bg, fg);
    if (blend_mode == BLEND_COLORDODGE) return blendColorDodge(bg, fg);
    if (blend_mode == BLEND_COLORBURN) return blendColorBurn(bg, fg);
    if (blend_mode == BLEND_HARDLIGHT) return blendHardLight(bg, fg);
    if (blend_mode == BLEND_SOFTLIGHT) return blendSoftLight(bg, fg);
    if (blend_mode == BLEND_DIFFERENCE) return blendDifference(bg, fg);
    if (blend_mode == BLEND_EXCLUSION) return blendExclusion(bg, fg);
    return fg;
}

void main() {
    vec4 fgAlbedo = texture(fg_albedo, texUVs);
    vec4 bgAlbedo = texture(bg_albedo, texUVs);

    float fgAlpha = clamp(fgAlbedo.a, 0.0, 1.0);
    float bgAlpha = clamp(bgAlbedo.a, 0.0, 1.0);

    vec3 blended = computeBlend(bgAlbedo.rgb, fgAlbedo.rgb);
    blended = mix(bgAlbedo.rgb, blended, fgAlpha);
    blended = clamp(blended, 0.0, 1.0);

    float outAlpha = clamp(fgAlpha + bgAlpha * (1.0 - fgAlpha), 0.0, 1.0);
    outAlbedo = vec4(blended, outAlpha);

    outEmissive = texture(fg_emissive, texUVs);
    outBump = texture(fg_bump, texUVs);
}
)NGSHADER";
}
