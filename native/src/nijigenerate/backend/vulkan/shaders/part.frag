#version 450
layout(constant_id = 0) const int blendEquation = 0;
layout(set = 0, binding = 0) uniform sampler2D albedoTex;
layout(set = 0, binding = 1) uniform sampler2D emissiveTex;
layout(set = 0, binding = 2) uniform sampler2D bumpTex;
layout(push_constant) uniform Push {
    vec4 tintOpacity;     // rgb=tint, a=opacity
    vec4 screenEmission;  // rgb=screen, a=emission
} pc;
layout(location = 0) in vec2 outUv;
layout(location = 0) out vec4 outColor;
void main() {
    vec4 albedo = texture(albedoTex, outUv);
    if (albedo.a <= 0.0001) {
        discard;
    }
    vec3 screen = vec3(1.0) - ((vec3(1.0)-albedo.rgb) * (vec3(1.0)-pc.screenEmission.rgb*albedo.a));
    // Emission is rendered separately and added after ambient-light modulation.
    outColor = vec4(clamp(screen * pc.tintOpacity.rgb, 0.0, 1.0), albedo.a) * pc.tintOpacity.a;
    // The portable reference mixes its blend result by coverage. Fold that
    // coverage into the source so fixed Multiply/Screen reproduce its RGB.
    if (blendEquation == 1 || blendEquation == 2) outColor.rgb *= outColor.a;
}
