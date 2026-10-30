#include "nijigenerate/backend/gl.hpp"
#include "nijigenerate/platform/gl_shader_compile.hpp"
#include "nijigenerate/backend/gl_state_internal.hpp"

namespace nijigenerate::backend::gl {

bool ngGLBackendCheckShader(GLuint shader, const char* description) {
    return platform::ngGLCheckShaderCompile(shader, description);
}

bool ngGLBackendCheckProgram(GLuint program, const char* description) {
    return platform::ngGLCheckProgramLink(program, description,
        detail::glBackendState().glslVersionString.data());
}

} // namespace nijigenerate::backend::gl
