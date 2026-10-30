#pragma once

#include "gpu_simple_hook_types.hpp"

namespace nijigenerate::commands::depth::bone_gpu_async::detail {

inline thread_local constinit NgDepthBoneGpuAsyncSupportHook supportHook{};
inline thread_local constinit NgDepthBoneGpuAsyncSubmitHook submitHook{};
inline thread_local constinit NgDepthBoneGpuAsyncPollHook pollHook{};
inline thread_local constinit NgDepthBoneGpuAsyncCancelHook cancelHook{};

} // namespace nijigenerate::commands::depth::bone_gpu_async::detail
