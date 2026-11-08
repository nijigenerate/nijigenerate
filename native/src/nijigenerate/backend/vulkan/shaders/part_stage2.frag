#version 450
layout(set = 0, binding = 0) uniform sampler2D albedoTex;
layout(set = 0, binding = 1) uniform sampler2D emissiveTex;
layout(push_constant) uniform Push {
    vec4 tintOpacity;
    vec4 screenEmission;
    vec4 emission; // x: source has already been premultiplied in a composite
} pc;
layout(location = 0) in vec2 outUv;
layout(location = 0) out vec4 outColor;
void main() {
    vec4 albedo = texture(albedoTex, outUv);
    vec4 emissive = texture(emissiveTex, outUv);
    if (albedo.a <= 0.0001) {
        discard;
    }
    float coverage = pc.emission.x > 0.5 ? 1.0 : albedo.a;
    vec3 screened = vec3(1.0) - ((vec3(1.0) - emissive.rgb) *
                                    (vec3(1.0) - pc.screenEmission.rgb * albedo.a));
    vec3 outE = screened * pc.tintOpacity.rgb * max(pc.screenEmission.a, 0.0) * coverage;
    outColor = vec4(clamp(outE, 0.0, 1.0), albedo.a) * pc.tintOpacity.a;
}
