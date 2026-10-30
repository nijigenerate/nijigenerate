#pragma once
// Embedded original resource bytes, including CRLF and UTF-8, in one static backing.
namespace nijigenerate::atlas::atlas::detail {
inline constexpr char vertexShader[] =
    "/*\r\n"
    "    Copyright \302\251 2020-2023, nijigenerate Project\r\n"
    "    Distributed under the 2-Clause BSD License, see LICENSE file.\r\n"
    "    \r\n"
    "    Authors: Luna Nielsen\r\n"
    "*/\r\n"
    "#version 330\r\n"
    "\r\n"
    "layout(location = 0) in vec2 verts;\r\n"
    "layout(location = 1) in vec2 uvs;\r\n"
    "out vec2 texUVs;\r\n"
    "\r\n"
    "uniform mat4 mvp;\r\n"
    "\r\n"
    "void main() {\r\n"
    "    gl_Position = mvp * vec4(verts.x, verts.y, 0, 1);\r\n"
    "    texUVs = uvs;\r\n"
    "}";
inline constexpr char fragmentShader[] =
    "/*\r\n"
    "    Copyright \302\251 2020-2023, nijigenerate Project\r\n"
    "    Distributed under the 2-Clause BSD License, see LICENSE file.\r\n"
    "    \r\n"
    "    Authors: Luna Nielsen\r\n"
    "*/\r\n"
    "#version 330\r\n"
    "in vec2 texUVs;\r\n"
    "out vec4 outAlbedo;\r\n"
    "\r\n"
    "uniform sampler2D albedo;\r\n"
    "uniform float opacity;\r\n"
    "\r\n"
    "void main() {\r\n"
    "    outAlbedo = texture(albedo, texUVs) * opacity;\r\n"
    "}";
}
