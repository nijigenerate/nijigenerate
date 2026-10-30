#pragma once

#include "gpu_indexed_binding.hpp"
#include <SDL3/SDL_video.h>
#include <bit>

namespace nijigenerate::commands::depth::bone_gpu_async::detail {

// The renderer supplies these entry points from its OpenGL context loader.
// Keeping the real SDL3 function-pointer types preserves the platform ABI.
struct IndexedTransformFeedbackApi {
    PFNGLGETINTEGERI_VPROC getIntegerIndexed{};
    PFNGLGETINTEGER64I_VPROC getInteger64Indexed{};
    PFNGLBINDBUFFERBASEPROC bindBufferBase{};
    PFNGLBINDBUFFERRANGEPROC bindBufferRange{};
};

template<class Function>
inline Function depthBoneGlProcedure(const char* name) {
    const auto procedure = SDL_GL_GetProcAddress(name);
    static_assert(sizeof(Function) == sizeof(procedure));
    return std::bit_cast<Function>(procedure);
}

inline IndexedTransformFeedbackBinding captureTransformFeedbackBinding(
    GLuint index, const IndexedTransformFeedbackApi& api) {
    IndexedTransformFeedbackBinding state;
    api.getIntegerIndexed(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, index, &state.buffer);
    api.getInteger64Indexed(GL_TRANSFORM_FEEDBACK_BUFFER_START, index, &state.start);
    api.getInteger64Indexed(GL_TRANSFORM_FEEDBACK_BUFFER_SIZE, index, &state.size);
    return state;
}

inline void restoreTransformFeedbackBinding(GLuint index, IndexedTransformFeedbackBinding state,
    const IndexedTransformFeedbackApi& api) {
    if (state.buffer == 0) {
        api.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, index, 0);
    } else if (state.size > 0) {
        api.bindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, index, static_cast<GLuint>(state.buffer),
            static_cast<GLintptr>(state.start), static_cast<GLsizeiptr>(state.size));
    } else {
        api.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, index, static_cast<GLuint>(state.buffer));
    }
}

// Calls require a current SDL GL context and the entry points checked by the
// original submission path. Missing procedures remain null, not fake functions.
inline IndexedTransformFeedbackBinding captureTransformFeedbackBinding(GLuint index) {
    IndexedTransformFeedbackApi api;
    api.getIntegerIndexed = depthBoneGlProcedure<PFNGLGETINTEGERI_VPROC>("glGetIntegeri_v");
    api.getInteger64Indexed = depthBoneGlProcedure<PFNGLGETINTEGER64I_VPROC>("glGetInteger64i_v");
    return captureTransformFeedbackBinding(index, api);
}

inline void restoreTransformFeedbackBinding(GLuint index, IndexedTransformFeedbackBinding state) {
    IndexedTransformFeedbackApi api;
    if (state.buffer != 0 && state.size > 0) {
        api.bindBufferRange = depthBoneGlProcedure<PFNGLBINDBUFFERRANGEPROC>("glBindBufferRange");
    } else {
        api.bindBufferBase = depthBoneGlProcedure<PFNGLBINDBUFFERBASEPROC>("glBindBufferBase");
    }
    restoreTransformFeedbackBinding(index, state, api);
}

} // namespace nijigenerate::commands::depth::bone_gpu_async::detail
