#include "nijigenerate/backend/gl.hpp"
#include "nijigenerate/backend/gl_state_internal.hpp"

namespace nijigenerate::backend::gl {

void ngGLBackendNewFrame() {
    if (!detail::glBackendState().shaderHandle) ngGLBackendCreateDeviceObjects();
}

} // namespace nijigenerate::backend::gl
