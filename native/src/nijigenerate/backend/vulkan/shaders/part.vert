#version 450
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUv;
layout(location = 2) in float inZ;
layout(location = 0) out vec2 outUv;
void main() {
    gl_Position = vec4(inPos, (inZ+1.0)*0.5, 1.0);
    outUv = inUv;
}
