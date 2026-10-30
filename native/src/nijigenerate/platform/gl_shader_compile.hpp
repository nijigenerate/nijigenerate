#pragma once
#include <cstdint>

namespace nijigenerate::platform {
bool ngGLCheckShaderCompile(std::uint32_t shader, const char* description);
bool ngGLCheckProgramLink(std::uint32_t program, const char* description, const char* glslVersion);
} // namespace nijigenerate::platform
