#pragma once
#include "runtime_gl_backend.hpp"

namespace nijigenerate::backend::runtime_gl {
// Call only after creating the application's current GL context, before loading puppets.
std::shared_ptr<OpenGLRenderBackend> ngInitializeOpenGLRenderer();
} // namespace nijigenerate::backend::runtime_gl
