// Copyright 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
#version 450
layout(set=0,binding=0) uniform sampler2D albedoTex;
layout(push_constant) uniform Push { vec4 tintOpacity; vec4 screenEmission; } pc;
layout(location=0) in vec2 outUv;
layout(location=0) out vec4 color;
layout(location=1) out vec4 emissionColor;
layout(location=2) out vec4 bumpColor;
void main() {
    if (texture(albedoTex,outUv).a <= pc.screenEmission.a) discard;
    color = emissionColor = bumpColor = vec4(1.0);
}
