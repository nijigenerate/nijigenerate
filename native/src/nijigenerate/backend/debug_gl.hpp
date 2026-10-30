#pragma once
#include <memory>
namespace nicxlive::core { class RenderBackend; }
namespace nijigenerate::core::dbg { class DebugProvider; }
namespace nijigenerate::backend::debug_gl {
void ngInstallOpenGLDebugProvider();
// Called only by an actual GL runtime backend's selection path, not by ImGui setup.
void ngBindOpenGLDebugProvider(std::shared_ptr<nicxlive::core::RenderBackend> runtimeBackend);
void ngReleaseOpenGLDebugProvider();
std::shared_ptr<core::dbg::DebugProvider> ngOpenGLDebugProvider();
}
