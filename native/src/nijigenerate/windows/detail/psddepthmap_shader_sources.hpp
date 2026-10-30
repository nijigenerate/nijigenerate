#pragma once

namespace nijigenerate::windows::psddepthmap::detail {

// Genuine shader assets from the independent editor, not host callbacks.
inline constexpr char psdDepthAdjustVertexSource[] = R"PSDGLSL(
#version 330
layout(location = 0) in vec3 documentPosition;
layout(location = 1) in vec2 uv;
out vec2 fragUv;
uniform vec2 viewportSize;
uniform vec2 center;
uniform vec4 yawPitch;
uniform vec4 zoomPan;
uniform float depthDisplayScale;
uniform vec4 layerDepthTransform;
void main() {
    float cy = yawPitch.x;
    float sy = yawPitch.y;
    float cp = yawPitch.z;
    float sp = yawPitch.w;
    float x = documentPosition.x - center.x;
    float y = documentPosition.y - center.y;
    float depth = documentPosition.z;
    if (abs(layerDepthTransform.x) > 0.000001) {
        depth = (depth - layerDepthTransform.y) *
            (layerDepthTransform.z / layerDepthTransform.x) + layerDepthTransform.w;
    } else {
        depth += layerDepthTransform.w - layerDepthTransform.y;
    }
    float z = -depth * depthDisplayScale;
    float rx = x * cy + z * sy;
    float rz = -x * sy + z * cy;
    float ry = y * cp - rz * sp;
    float cameraDepth = y * sp + rz * cp;
    vec2 screen = viewportSize * 0.5 + vec2(rx, ry) * zoomPan.x + zoomPan.yz;
    vec2 clip = vec2(screen.x / viewportSize.x * 2.0 - 1.0, 1.0 - screen.y / viewportSize.y * 2.0);
    float zClip = clamp(cameraDepth / max(depthDisplayScale * 2.0, 1.0), -1.0, 1.0);
    gl_Position = vec4(clip, zClip, 1.0);
    fragUv = uv;
}
)PSDGLSL";

inline constexpr char psdDepthAdjustFragmentSource[] = R"PSDGLSL(
#version 330
in vec2 fragUv;
out vec4 color;
uniform sampler2D tex;
void main() {
    color = texture(tex, fragUv);
    if (color.a < 0.01) discard;
}
)PSDGLSL";

} // namespace nijigenerate::windows::psddepthmap::detail
