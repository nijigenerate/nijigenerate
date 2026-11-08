#version 450
layout(set = 0, binding = 0) uniform sampler2D albedoTex;
layout(push_constant) uniform Push {
    vec4 tintOpacity;
    vec4 screenEmission;
} pc;
layout(location = 0) in vec2 outUv;
layout(location = 0) out vec4 outColor;
void main() {
    vec4 albedo = texture(albedoTex, outUv);
    if (albedo.a <= 0.0001) {
        discard;
    }
    vec3 screen = vec3(1.0) - ((vec3(1.0)-albedo.rgb) * (vec3(1.0)-pc.screenEmission.rgb*albedo.a));
    outColor = vec4(clamp(screen * pc.tintOpacity.rgb, 0.0, 1.0), albedo.a) * pc.tintOpacity.a;
}
