#pragma once

#include <SDL3/SDL_opengl.h>
#include <compat/object_metadata.hpp>

namespace nijigenerate::commands::depth::bone_gpu_async::detail {

struct IndexedTransformFeedbackBinding {
    GLint buffer{};
    GLint64 start{};
    GLint64 size{};
};

} // namespace nijigenerate::commands::depth::bone_gpu_async::detail

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
    ::nijigenerate::commands::depth::bone_gpu_async::detail::IndexedTransformFeedbackBinding> {
    inline static constexpr bool isNativeObject=false;
    inline static constexpr char qualifiedArgument[]=
        "(nijigenerate.commands.depth.bone_gpu_async.IndexedTransformFeedbackBinding)";
    inline static constexpr char unqualifiedArgument[]="(IndexedTransformFeedbackBinding)";
    inline static constexpr char qualifiedElement[]=
        "nijigenerate.commands.depth.bone_gpu_async.IndexedTransformFeedbackBinding";
    inline static constexpr char unqualifiedElement[]="IndexedTransformFeedbackBinding";
};
} // namespace nicxlive::core::d_runtime
