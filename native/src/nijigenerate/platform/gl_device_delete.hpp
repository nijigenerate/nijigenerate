#pragma once
#include <cstdint>

namespace nijigenerate::platform {
void ngGLDeleteBackendBuffer(std::uint32_t buffer);
void ngGLDetachBackendShader(std::uint32_t program, std::uint32_t shader);
void ngGLDeleteBackendShader(std::uint32_t shader);
void ngGLDeleteBackendProgram(std::uint32_t program);
} // namespace nijigenerate::platform
