// Copyright 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
#include <metal_stdlib>
using namespace metal;
struct VertexIn { float2 position [[attribute(0)]]; float2 uv [[attribute(1)]]; float z [[attribute(2)]]; };
struct Varying { float4 position [[position]]; float2 uv; };
struct Parameters { float4 tint; float4 screen; float4 emission; uint mode; uint kind; uint offscreen; uint padding; };
vertex Varying sceneVertex(VertexIn v [[stage_in]], constant Parameters& p [[buffer(1)]]) {
    Varying out; out.position=float4(v.position.x,p.offscreen?-v.position.y:v.position.y,(v.z+1)*.5,1); out.uv=v.uv; return out;
}
struct SceneOutput {
#if NG_FRAMEBUFFER_FETCH
    float4 color [[color(0), raster_order_group(0)]];
#else
    float4 color [[color(0)]];
#endif
    float4 emission [[color(1)]];
    float4 bump [[color(2)]];
};
constant int BLEND_NORMAL      = 0;
constant int BLEND_MULTIPLY    = 1;
constant int BLEND_SCREEN      = 2;
constant int BLEND_OVERLAY     = 3;
constant int BLEND_DARKEN      = 4;
constant int BLEND_LIGHTEN     = 5;
constant int BLEND_COLORDODGE  = 6;
constant int BLEND_LINEARDODGE = 7; // unused here, handled by legacy pipeline
constant int BLEND_ADDGLOW     = 8; // unused here, handled by legacy pipeline
constant int BLEND_COLORBURN   = 9;
constant int BLEND_HARDLIGHT   = 10;
constant int BLEND_SOFTLIGHT   = 11;
constant int BLEND_DIFFERENCE  = 12;
constant int BLEND_EXCLUSION   = 13;

float3 blendMultiply(float3 bg, float3 fg) {
    return bg * fg;
}

float3 blendScreen(float3 bg, float3 fg) {
    return bg + fg - bg * fg;
}

float3 blendOverlay(float3 bg, float3 fg) {
    float3 dark = 2.0 * bg * fg;
    float3 light = 1.0 - 2.0 * (1.0 - bg) * (1.0 - fg);
    bool3 mask = (bg < float3(0.5));
    return float3(
        mask.x ? dark.x : light.x,
        mask.y ? dark.y : light.y,
        mask.z ? dark.z : light.z
    );
}

float3 blendDarken(float3 bg, float3 fg) {
    return min(bg, fg);
}

float3 blendLighten(float3 bg, float3 fg) {
    return max(bg, fg);
}

float3 blendColorDodge(float3 bg, float3 fg) {
    float3 denom = max(float3(1e-5), 1.0 - fg);
    return clamp(bg / denom, 0.0, 1.0);
}

float3 blendColorBurn(float3 bg, float3 fg) {
    float3 denom = max(float3(1e-5), fg);
    return 1.0 - clamp((1.0 - bg) / denom, 0.0, 1.0);
}

float3 blendHardLight(float3 bg, float3 fg) {
    float3 dark = 2.0 * bg * fg;
    float3 light = 1.0 - 2.0 * (1.0 - bg) * (1.0 - fg);
    bool3 mask = (fg < float3(0.5));
    return float3(
        mask.x ? dark.x : light.x,
        mask.y ? dark.y : light.y,
        mask.z ? dark.z : light.z
    );
}

float3 blendSoftLight(float3 bg, float3 fg) {
    float3 sqrtBg = sqrt(clamp(bg, 0.0, 1.0));
    float3 dark = bg - (1.0 - 2.0 * fg) * bg * (1.0 - bg);
    float3 light = bg + (2.0 * fg - 1.0) * (sqrtBg - bg);
    bool3 mask = (fg < float3(0.5));
    return float3(
        mask.x ? dark.x : light.x,
        mask.y ? dark.y : light.y,
        mask.z ? dark.z : light.z
    );
}

float3 blendDifference(float3 bg, float3 fg) {
    return abs(bg - fg);
}

float3 blendExclusion(float3 bg, float3 fg) {
    return bg + fg - 2.0 * bg * fg;
}

float3 computeBlend(float3 bg, float3 fg, int mode) {
    if (mode == BLEND_MULTIPLY) return blendMultiply(bg, fg);
    if (mode == BLEND_SCREEN) return blendScreen(bg, fg);
    if (mode == BLEND_OVERLAY) return blendOverlay(bg, fg);
    if (mode == BLEND_DARKEN) return blendDarken(bg, fg);
    if (mode == BLEND_LIGHTEN) return blendLighten(bg, fg);
    if (mode == BLEND_COLORDODGE) return blendColorDodge(bg, fg);
    if (mode == BLEND_COLORBURN) return blendColorBurn(bg, fg);
    if (mode == BLEND_HARDLIGHT) return blendHardLight(bg, fg);
    if (mode == BLEND_SOFTLIGHT) return blendSoftLight(bg, fg);
    if (mode == BLEND_DIFFERENCE) return blendDifference(bg, fg);
    if (mode == BLEND_EXCLUSION) return blendExclusion(bg, fg);
    return fg;
}



bool advancedMode(uint m) { return (m>=1 && m<=6)||(m>=9 && m<=13); }
float4 sourceColor(float4 a,constant Parameters& p) {
    float3 screened=1-((1-a.rgb)*(1-p.screen.rgb*a.a));
    return float4(clamp(screened*p.tint.rgb,0.,1.),a.a)*p.tint.a;
}
float4 advancedColor(float4 src,float4 dst,uint mode) {
    return float4(clamp(mix(dst.rgb,computeBlend(dst.rgb,src.rgb,int(mode)),clamp(src.a,0.,1.)),0.,1.),
        clamp(src.a+dst.a*(1-src.a),0.,1.));
}
fragment SceneOutput sceneFragment(Varying in [[stage_in]],
#if NG_FRAMEBUFFER_FETCH
    float4 destination [[color(0), raster_order_group(0)]],
#endif
    constant Parameters& p [[buffer(0)]],
    texture2d<float> albedo [[texture(0)]], texture2d<float> emission [[texture(1)]], texture2d<float> bump [[texture(2)]], texture2d<float> background [[texture(3)]],
    sampler s0 [[sampler(0)]],sampler s1 [[sampler(1)]],sampler s2 [[sampler(2)]]) {
#if !NG_FRAMEBUFFER_FETCH
    float4 destination=advancedMode(p.mode)?background.read(uint2(in.position.xy)):float4(0);
#endif
    float4 a=albedo.sample(s0,in.uv);
    if(p.kind==3 || p.kind==6 || p.kind==7 || p.kind==8) {
        if(p.kind!=8 && a.a<=p.screen.a) discard_fragment();
        return {float4(1),float4(1),float4(1)};
    }
    if(a.a<=.0001) discard_fragment();
    float4 color=sourceColor(a,p);
    if(advancedMode(p.mode)) color=advancedColor(color,destination,p.mode);
    float4 e=emission.sample(s1,in.uv);
    float coverage=p.emission.x>.5?1.:a.a;
    float3 screened=1-((1-e.rgb)*(1-p.screen.rgb*a.a));
    float4 ec=float4(clamp(screened*p.tint.rgb*max(p.screen.a,0.)*coverage,0.,1.),a.a)*p.tint.a;
    return {color,ec,bump.sample(s2,in.uv)*a.a*p.tint.a};
}
fragment float4 singleFragment(Varying in [[stage_in]],
#if NG_FRAMEBUFFER_FETCH
    float4 destination [[color(0), raster_order_group(0)]],
#endif
    constant Parameters& p [[buffer(0)]],
    texture2d<float> source [[texture(0)]],texture2d<float> emissive [[texture(1)]], texture2d<float> background [[texture(3)]],
    sampler s0 [[sampler(0)]],sampler s1 [[sampler(1)]]) {
#if !NG_FRAMEBUFFER_FETCH
    float4 destination=advancedMode(p.mode)?background.read(uint2(in.position.xy)):float4(0);
#endif
    float2 uv=in.uv;
    if(p.kind==9) {
        float4 color=source.sample(s0,clamp(uv,0.,1.))*.2270270270;
        color+=source.sample(s0,clamp(uv+p.screen.xy*1.3846153846,0.,1.))*.3162162162;
        color+=source.sample(s0,clamp(uv-p.screen.xy*1.3846153846,0.,1.))*.3162162162;
        color+=source.sample(s0,clamp(uv+p.screen.xy*3.2307692308,0.,1.))*.0702702703;
        color+=source.sample(s0,clamp(uv-p.screen.xy*3.2307692308,0.,1.))*.0702702703;
        return color;
    }
    if(p.kind==13) {
        float4 blurred=0;
        for(int i=0;i<36;++i) {
            float2 d=float2(i%6,i/6)*4.-12.5;
            float2 n=d/6.25;
            float g=exp(-.5*dot(n,n))/(6.28*6.25*6.25);
            blurred+=g*emissive.sample(s1,uv+p.screen.xy*d,level(2));
        }
        if(blurred.a>0) blurred/=blurred.a;
        return source.sample(s0,uv)*(float4(p.tint.rgb,1)+emissive.sample(s1,uv)+blurred);
    }
    if(p.kind==11) return float4(source.sample(s0,uv).rgb,0);
    float4 a=source.sample(s0,uv);
    if(p.kind==2) {
        if(a.a<=.0001) discard_fragment();
        float3 e=emissive.sample(s1,uv).rgb;
        float3 screened=1-((1-e)*(1-p.screen.rgb*a.a));
        float coverage=p.emission.x>.5?1.:a.a;
        return float4(clamp(screened*p.tint.rgb*max(p.screen.a,0.)*coverage,0.,1.),a.a)*p.tint.a;
    }
    if(p.kind==3 || p.kind==6 || p.kind==7 || p.kind==8) {
        if(p.kind!=8 && a.a<=p.screen.a) discard_fragment();
        return float4(1);
    }
    if(a.a<=.0001) discard_fragment();
    float4 color=sourceColor(a,p);
    if(advancedMode(p.mode)) color=advancedColor(color,destination,p.mode);
    return color;
}
