#pragma once
// Independent nijilive OpenGL shader_backend.d port (BSD-2-Clause).
#include <math/mat4.hpp>
#include <string_view>

namespace nijigenerate::backend::runtime_gl {
struct ShaderProgramHandle {
    unsigned program = 0;
    unsigned vert = 0;
    unsigned frag = 0;
};
void oglCreateShaderProgram(ShaderProgramHandle&, std::string_view vertex, std::string_view fragment);
void oglDestroyShaderProgram(ShaderProgramHandle&);
void oglUseShaderProgram(ShaderProgramHandle&);
int oglShaderGetUniformLocation(ShaderProgramHandle&, std::string_view name);
void oglSetUniformBool(int, bool);
void oglSetUniformInt(int, int);
void oglSetUniformFloat(int, float);
void oglSetUniformVec2(int, nicxlive::core::math::Vec2);
void oglSetUniformVec3(int, nicxlive::core::math::Vec3);
void oglSetUniformVec4(int, nicxlive::core::math::Vec4);
void oglSetUniformMat4(int, nicxlive::core::math::Mat4);
}
