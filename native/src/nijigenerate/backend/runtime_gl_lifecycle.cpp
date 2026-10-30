#include "runtime_gl_lifecycle.hpp"
#include <runtime_state.hpp>

namespace nijigenerate::backend::runtime_gl {
std::shared_ptr<OpenGLRenderBackend> ngInitializeOpenGLRenderer() {
    auto backend = ngSelectOpenGLRenderBackend();
    nicxlive::core::initRenderer();
    return backend;
}
} // namespace nijigenerate::backend::runtime_gl
