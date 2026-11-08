#version 450
layout(set = 0, binding = 0) uniform sampler2D albedoTex;
layout(push_constant) uniform Push {
    vec4 tintOpacity;
    vec4 screenEmission; // a=threshold
} pc;
layout(location = 0) in vec2 outUv;
layout(location = 0) out vec4 outColor;
void main() {
    vec4 albedo = texture(albedoTex, outUv);
    if (albedo.a <= pc.screenEmission.a) discard;
    outColor = vec4(1.0, 1.0, 1.0, 1.0);
}
