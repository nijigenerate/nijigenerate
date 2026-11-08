#version 450
layout(set = 0, binding = 0) uniform sampler2D emissionTex;
layout(location = 0) in vec2 outUv;
layout(location = 0) out vec4 outColor;
void main() {
    // The blend state adds RGB and preserves the albedo target's alpha.
    outColor = vec4(texture(emissionTex, outUv).rgb, 0.0);
}
