#pragma once
#include <nijigenerate/backend/runtime_gl_backend.hpp>
#include <render/backend_queue.hpp>
#include <runtime_state.hpp>
#include <stdexcept>

namespace nijigenerate::viewport::depth::detail {
// Independent Shader facade operations used by the depth renderer. Backend
// selection follows the live runtime; a UI GL context never selects a backend.
class DepthRendererShader {
    using GL = backend::runtime_gl::OpenGLRenderBackend;
    struct QueueShaderHandle : backend::runtime_gl::RenderShaderHandle {};
    std::shared_ptr<backend::runtime_gl::RenderShaderHandle> handle;
    static bool queue(const std::shared_ptr<nicxlive::core::RenderBackend>& backend) {
        return bool(std::dynamic_pointer_cast<nicxlive::core::QueueRenderBackend>(backend));
    }
    static std::shared_ptr<GL> gl(const std::shared_ptr<nicxlive::core::RenderBackend>& backend) {
        auto result = std::dynamic_pointer_cast<GL>(backend);
        if (!result) throw std::runtime_error("Selected render backend does not provide shader operations");
        return result;
    }
public:
    DepthRendererShader(std::string_view vertex, std::string_view fragment) {
        const auto backend = nicxlive::core::currentRenderBackend();
        if (queue(backend)) handle = std::make_shared<QueueShaderHandle>();
        else handle = gl(backend)->createShader(vertex, fragment);
    }
    ~DepthRendererShader() {
        if (!handle) return;
        const auto backend = nicxlive::core::tryRenderBackend();
        if (backend && !queue(backend)) gl(backend)->destroyShader(handle);
        handle.reset();
    }
    void use() {
        if (!handle) return; const auto backend = nicxlive::core::currentRenderBackend();
        if (!queue(backend)) gl(backend)->useShader(handle);
    }
    int getUniformLocation(std::string_view name) {
        if (!handle) return -1; const auto backend = nicxlive::core::currentRenderBackend();
        return queue(backend) ? -1 : gl(backend)->getShaderUniformLocation(handle, name);
    }
    template<class T> void setUniform(int location, T value) {
        if (!handle) return; const auto backend = nicxlive::core::currentRenderBackend();
        if (!queue(backend)) gl(backend)->setShaderUniform(handle, location, value);
    }
};
}
