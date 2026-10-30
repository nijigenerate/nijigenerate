#pragma once
// Independent nijilive scene shaders, BSD-2-Clause.
namespace nijigenerate::backend::runtime_gl::scene_shaders {
inline constexpr const char* vertex = R"NGGL(/*
    Copyright © 2020, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
#version 330
uniform mat4 mvp;
layout(location = 0) in vec2 verts;
layout(location = 1) in vec2 uvs;

out vec2 texUVs;

void main() {
    gl_Position = mvp * vec4(verts.x, verts.y, 0, 1);
    texUVs = uvs;
})NGGL";
inline constexpr const char* scene = R"NGGL(/*
    Copyright © 2020, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
#version 330
in vec2 texUVs;
out vec4 outColor;

uniform sampler2D fbo;

void main() {
    // Set color to the corrosponding pixel in the FBO
    vec4 color = texture(fbo, texUVs);
    outColor = vec4(color.r, color.g, color.b, color.a);
})NGGL";
inline constexpr const char* lighting = R"NGGL(/*
    Copyright © 2020, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
#version 330
in vec2 texUVs;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outEmissive;
layout(location = 2) out vec4 outBump;

uniform vec3 ambientLight;
uniform vec2 fbSize;

uniform sampler2D albedo;
uniform sampler2D emissive;
uniform sampler2D bumpmap;
uniform int LOD = 2;
uniform int samples = 25;

// Gaussian
float gaussian(vec2 i, float sigma) {
    return exp(-0.5*dot(i /= sigma, i)) / (6.28*sigma*sigma);
}

// Bloom texture by blurring it
vec4 bloom(sampler2D sp, vec2 uv, vec2 scale) {
    float sigma = float(samples) * 0.25;
    vec4 out_ = vec4(0);
    int sLOD = 1 << LOD;
    int s = samples/sLOD;
    
    for ( int i = 0; i < s*s; i++ ) {
        vec2 d = vec2(i%s, i/s)*float(sLOD) - float(samples)/2.0;
        out_ += gaussian(d, sigma) * textureLod( sp, uv + scale * d, LOD);
    }
    
    return out_ / out_.a;
}

void main() {

    // Bloom
    outEmissive = texture(emissive, texUVs)+bloom(emissive, texUVs, 1.0/fbSize);

    // Set color to the corrosponding pixel in the FBO
    vec4 light = vec4(ambientLight, 1) + outEmissive;

    outAlbedo = (texture(albedo, texUVs)*light);
    outBump = texture(bumpmap, texUVs);
})NGGL";
}
