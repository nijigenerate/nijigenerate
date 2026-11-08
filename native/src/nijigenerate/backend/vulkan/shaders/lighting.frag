// Copyright (c) 2020 Inochi2D Project; (c) 2024 nijigenerate Project. BSD-2-Clause.
#version 450
layout(set=0,binding=0) uniform sampler2D albedo;
layout(set=0,binding=1) uniform sampler2D emissive;
layout(push_constant) uniform Push { vec4 ambient; vec4 inverseSize; vec4 extra; } pc;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
float gaussian(vec2 v,float sigma) { v/=sigma; return exp(-.5*dot(v,v))/(6.28*sigma*sigma); }
void main() {
    vec4 blurred=vec4(0);
    for(int i=0;i<36;++i) {
        vec2 d=vec2(i%6,i/6)*4.0-12.5;
        blurred+=gaussian(d,6.25)*textureLod(emissive,uv+pc.inverseSize.xy*d,2);
    }
    if(blurred.a>0) blurred/=blurred.a;
    vec4 emission=texture(emissive,uv)+blurred;
    color=texture(albedo,uv)*(vec4(pc.ambient.rgb,1)+emission);
}
