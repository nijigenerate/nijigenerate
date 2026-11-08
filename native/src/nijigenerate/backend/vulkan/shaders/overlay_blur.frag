// nijiexpose windows/main.cpp overlay blur, same taps and weights.
#version 450
layout(set=0,binding=0) uniform sampler2D sourceTexture;
layout(push_constant) uniform Push {vec4 tintOpacity;vec4 texelStep;} pc;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
vec4 sampleAt(vec2 p){return texture(sourceTexture,clamp(p,vec2(0),vec2(1)));}
void main(){
    color=sampleAt(uv)*0.2270270270;
    color+=sampleAt(uv+pc.texelStep.xy*1.3846153846)*0.3162162162;
    color+=sampleAt(uv-pc.texelStep.xy*1.3846153846)*0.3162162162;
    color+=sampleAt(uv+pc.texelStep.xy*3.2307692308)*0.0702702703;
    color+=sampleAt(uv-pc.texelStep.xy*3.2307692308)*0.0702702703;
}
