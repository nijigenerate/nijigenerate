#pragma once

// Shader text from the independently authored nijigenerate editor gl.d.
// Copyright 2020-2023 ImGui & Inochi2D Project; 2024 nijigenerate Project.
// Distributed under the MIT license; see the ImGui LICENSE file.
namespace nijigenerate::backend::gl::detail {
inline constexpr char vertex_shader_glsl_120[] = "uniform mat4 ProjMtx;\nattribute vec2 Position;\nattribute vec2 UV;\nattribute vec4 Color;\nvarying vec2 Frag_UV;\nvarying vec4 Frag_Color;\nvoid main()\n{\n    Frag_UV = UV;\n    Frag_Color = Color;\n    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n}\n";
inline constexpr char vertex_shader_glsl_130[] = "\n    uniform mat4 ProjMtx;\n    in vec2 Position;\n    in vec2 UV;\n    in vec4 Color;\n    out vec2 Frag_UV;\n    out vec4 Frag_Color;\n    void main()\n    {\n        Frag_UV = UV;\n        Frag_Color = Color;\n        gl_Position = ProjMtx * vec4(Position.xy,0,1);\n    }\n    ";
inline constexpr char vertex_shader_glsl_300_es[] = "precision mediump float;\nlayout (location = 0) in vec2 Position;\nlayout (location = 1) in vec2 UV;\nlayout (location = 2) in vec4 Color;\nuniform mat4 ProjMtx;\nout vec2 Frag_UV;\nout vec4 Frag_Color;\nvoid main()\n{\n    Frag_UV = UV;\n    Frag_Color = Color;\n    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n}\n";
inline constexpr char vertex_shader_glsl_410_core[] = "layout (location = 0) in vec2 Position;\nlayout (location = 1) in vec2 UV;\nlayout (location = 2) in vec4 Color;\nuniform mat4 ProjMtx;\nout vec2 Frag_UV;\nout vec4 Frag_Color;\nvoid main()\n{\n    Frag_UV = UV;\n    Frag_Color = Color;\n    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n}\n";
inline constexpr char fragment_shader_glsl_120[] = "#ifdef GL_ES\n    precision mediump float;\n#endif\nuniform sampler2D Texture;\nvarying vec2 Frag_UV;\nvarying vec4 Frag_Color;\nvoid main()\n{\n    gl_FragColor = Frag_Color * texture2D(Texture, Frag_UV.st);\n}\n";
inline constexpr char fragment_shader_glsl_130[] = "\n    uniform sampler2D Texture;\n    in vec2 Frag_UV;\n    in vec4 Frag_Color;\n    out vec4 Out_Color;\n    void main()\n    {\n        Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n    }\n    ";
inline constexpr char fragment_shader_glsl_300_es[] = "precision mediump float;\nuniform sampler2D Texture;\nin vec2 Frag_UV;\nin vec4 Frag_Color;\nlayout (location = 0) out vec4 Out_Color;\nvoid main()\n{\n    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n}\n";
inline constexpr char fragment_shader_glsl_410_core[] = "in vec2 Frag_UV;\nin vec4 Frag_Color;\nuniform sampler2D Texture;\nlayout (location = 0) out vec4 Out_Color;\nvoid main()\n{\n    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n}\n";
} // namespace nijigenerate::backend::gl::detail
