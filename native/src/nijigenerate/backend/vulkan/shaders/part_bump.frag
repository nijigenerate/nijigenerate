#version 450
layout(set=0,binding=0) uniform sampler2D albedoTex;
layout(set=0,binding=2) uniform sampler2D bumpTex;
layout(push_constant) uniform Push { vec4 tintOpacity; vec4 screenEmission; vec4 emission; } pc;
layout(location=0) in vec2 outUv;
layout(location=0) out vec4 outColor;
void main() {
    vec4 albedo=texture(albedoTex,outUv);
    if(albedo.a<=0.0001) discard;
    outColor=texture(bumpTex,outUv)*albedo.a*pc.tintOpacity.a;
}
