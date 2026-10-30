#include "nijigenerate/backend/gl.hpp"

namespace nijigenerate::backend::gl {

void ngGLBackendShutdown() {
    ngGLBackendPlatformInterfaceShutdown();
    ngGLBackendDestroyDeviceObjects();
}

} // namespace nijigenerate::backend::gl
