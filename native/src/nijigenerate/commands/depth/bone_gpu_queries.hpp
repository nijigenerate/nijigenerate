#pragma once

#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <math/veca.hpp>

namespace nijigenerate::commands::depth::bone {

bool ngDepthBoneGpuSupported();
compat::RetainedString ngDepthBoneGpuSupportDiagnostic();
nicxlive::core::math::Vec2Array ngDepthBoneGpuReadbackToOffsets(
    compat::Slice<float> xs, compat::Slice<float> ys);

namespace detail {
void enforceDepthBoneGpuAvailable(compat::RetainedStringSlice context);
}

} // namespace nijigenerate::commands::depth::bone
