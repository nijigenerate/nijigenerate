#pragma once

#include <nijigenerate/compat/slice.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::commands::depth::bone_gpu_async {

struct NgDepthBoneGpuAsyncResult {
    bool ready{};
    compat::Slice<float> xs;
    compat::Slice<float> ys;
};

} // namespace nijigenerate::commands::depth::bone_gpu_async

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
    ::nijigenerate::commands::depth::bone_gpu_async::NgDepthBoneGpuAsyncResult> {
    inline static constexpr bool isNativeObject=false;
    inline static constexpr char qualifiedArgument[]=
        "(nijigenerate.commands.depth.bone_gpu_async.NgDepthBoneGpuAsyncResult)";
    inline static constexpr char unqualifiedArgument[]="(NgDepthBoneGpuAsyncResult)";
    inline static constexpr char qualifiedElement[]=
        "nijigenerate.commands.depth.bone_gpu_async.NgDepthBoneGpuAsyncResult";
    inline static constexpr char unqualifiedElement[]="NgDepthBoneGpuAsyncResult";
};
} // namespace nicxlive::core::d_runtime
